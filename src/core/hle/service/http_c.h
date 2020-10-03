// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <curl/curl.h>
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
} // namespace Core

namespace Service::HTTP {

enum class RequestMethod : u8 {
    None = 0x0,
    Get = 0x1,
    Post = 0x2,
    Head = 0x3,
    Put = 0x4,
    Delete = 0x5,
    PostEmpty = 0x6,
    PutEmpty = 0x7,
};

/// The number of request methods, any valid method must be less than this.
constexpr u32 TotalRequestMethods = 8;

enum class RequestState : u8 {
    NotStarted = 0x1,             // Request has not started yet.
    InProgress = 0x5,             // Request in progress, sending request over the network.
    ReadyToDownloadContent = 0x7, // Ready to download the content. (needs verification)
    ReadyToDownload = 0x8,        // Ready to download?
    TimedOut = 0xA,               // Request timed out?
};

/// Represents a client certificate along with its private key, stored as a byte array of DER data.
/// There can only be at most one client certificate context attached to an HTTP context at any
/// given time.
struct ClientCertContext {
    using Handle = u32;
    Handle handle;
    u32 session_id;
    u8 cert_id;
    std::vector<u8> certificate;
    std::vector<u8> private_key;
};

/// Represents a root certificate chain, it contains a list of DER-encoded certificates for
/// verifying HTTP requests. An HTTP context can have at most one root certificate chain attached to
/// it, but the chain may contain an arbitrary number of certificates in it.
struct RootCertChain {
    struct RootCACert {
        using Handle = u32;
        Handle handle;
        u32 session_id;
        std::vector<u8> certificate;
    };

    using Handle = u32;
    Handle handle;
    u32 session_id;
    std::vector<RootCACert> certificates;
};

/// Represents an HTTP context.
class Context final {
public:
    using Handle = u32;

    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    ~Context();

    void MakeRequest();

    struct Proxy {
        std::string url;
        std::string username;
        std::string password;
        u16 port;
    };

    struct BasicAuth {
        std::string username;
        std::string password;
    };

    struct RequestHeader {
        explicit RequestHeader(std::string name, std::string value) : name(name), value(value) {}
        std::string name;
        std::string value;
    };

    struct PostData {
        enum class Type {
            Ascii,
            Binary,
            Raw,
        };

        PostData(std::string name, std::string value, Type type)
            : name(name), value(value), type(type) {}

        std::string name;
        std::string value;
        Type type;
    };

    struct SSLConfig {
        u32 options;
        std::weak_ptr<ClientCertContext> client_cert_ctx;
        std::weak_ptr<RootCertChain> root_ca_chain;
    };

    Handle handle;
    u32 session_id;
    std::string url;
    RequestMethod method;
    std::atomic<RequestState> state = RequestState::NotStarted;
    std::optional<Proxy> proxy;
    std::optional<BasicAuth> basic_auth;
    SSLConfig ssl_config{};
    u32 socket_buffer_size;
    std::vector<RequestHeader> request_headers;
    std::unordered_map<std::string, std::string> response_headers;
    std::vector<PostData> post_data;
    u32 current_offset = 0;
    std::string request_body;
    std::string response_body;
    CURL* curl = nullptr;
    curl_slist* request_headers_slist = nullptr;
};

struct SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
    /// The HTTP context that is currently bound to this session, this can be empty if no context
    /// has been bound. Certain commands can only be called on a session with a bound context.
    std::optional<Context::Handle> current_http_context;

    u32 session_id;

    /// Number of HTTP contexts that are currently opened in this session.
    u32 num_http_contexts = 0;
    /// Number of ClientCert contexts that are currently opened in this session.
    u32 num_client_certs = 0;

    /// Whether this session has been initialized in some way, be it via Initialize or
    /// InitializeConnectionSession.
    bool initialized = false;
};

class HTTP_C final : public ServiceFramework<HTTP_C, SessionData> {
public:
    HTTP_C();

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void CreateContext(Kernel::HLERequestContext& ctx);
    void CloseContext(Kernel::HLERequestContext& ctx);
    void GetRequestState(Kernel::HLERequestContext& ctx);
    void GetDownloadSizeState(Kernel::HLERequestContext& ctx);
    void InitializeConnectionSession(Kernel::HLERequestContext& ctx);
    void BeginRequest(Kernel::HLERequestContext& ctx);
    void BeginRequestAsync(Kernel::HLERequestContext& ctx);
    void ReceiveData(Kernel::HLERequestContext& ctx);
    void ReceiveDataTimeout(Kernel::HLERequestContext& ctx);
    void AddRequestHeader(Kernel::HLERequestContext& ctx);
    void AddPostDataAscii(Kernel::HLERequestContext& ctx);
    void AddPostDataBinary(Kernel::HLERequestContext& ctx);
    void AddPostDataRaw(Kernel::HLERequestContext& ctx);
    void GetResponseHeader(Kernel::HLERequestContext& ctx);
    void GetResponseStatusCode(Kernel::HLERequestContext& ctx);
    void GetResponseStatusCodeTimeout(Kernel::HLERequestContext& ctx);
    void SetClientCertContext(Kernel::HLERequestContext& ctx);
    void GetSSLError(Kernel::HLERequestContext& ctx);
    void OpenClientCertContext(Kernel::HLERequestContext& ctx);
    void OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx);
    void CloseClientCertContext(Kernel::HLERequestContext& ctx);
    void Finalize(Kernel::HLERequestContext& ctx);

    void DecryptClCertA();

    std::shared_ptr<Kernel::SharedMemory> shared_memory = nullptr;

    /// The next number to use when a new HTTP session is initalized.
    u32 session_counter = 0;

    /// The next handle number to use when a new HTTP context is created.
    Context::Handle context_counter = 0;

    /// The next handle number to use when a new ClientCert context is created.
    ClientCertContext::Handle client_certs_counter = 0;

    /// Global list of HTTP contexts currently opened.
    std::unordered_map<Context::Handle, Context> contexts;

    /// Global list of  ClientCert contexts currently opened.
    std::unordered_map<ClientCertContext::Handle, std::shared_ptr<ClientCertContext>> client_certs;

    struct {
        std::vector<u8> certificate;
        std::vector<u8> private_key;
        bool init = false;
    } ClCertA;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::HTTP
