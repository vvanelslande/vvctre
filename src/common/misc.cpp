// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdlib>
#ifdef _WIN32
#include <wincrypt.h>
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#endif
#include <curl/curl.h>
#include <mbedtls/ssl.h>
#include "common/common_funcs.h"
#include "common/file_util.h"

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
std::string GetLastErrorMsg() {
    static const std::size_t buff_size = 255;
    char err_str[buff_size];

#ifdef _WIN32
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err_str, buff_size, nullptr);
#else
    // Thread safe (XSI-compliant)
    strerror_r(errno, err_str, buff_size);
#endif

    return std::string(err_str, buff_size);
}

namespace Common {

void* CreateCertificateChainWithSystemCertificates() {
    mbedtls_x509_crt* chain = (mbedtls_x509_crt*)calloc(1, sizeof(mbedtls_x509_crt));
    if (chain == nullptr) {
        return nullptr;
    }
    mbedtls_x509_crt_init(chain);

#ifdef _WIN32
    HCERTSTORE store = CertOpenSystemStore((HCRYPTPROV)NULL, L"ROOT");

    if (store == nullptr) {
        mbedtls_x509_crt_free(chain);
        free(chain);
        return nullptr;
    }

    PCCERT_CONTEXT cert = NULL;

    while (cert = CertEnumCertificatesInStore(store, cert)) {
        mbedtls_x509_crt_parse_der(chain, (unsigned char*)cert->pbCertEncoded, cert->cbCertEncoded);
    }

    CertFreeCertificateContext(cert);
    CertCloseStore(store, 0);
#else
    if (FileUtil::Exists("/etc/ssl/certs/ca-certificates.crt")) {
        mbedtls_x509_crt_parse_file(chain, "/etc/ssl/certs/ca-certificates.crt");
    } else if (FileUtil::Exists("/etc/pki/tls/certs/ca-bundle.crt")) {
        mbedtls_x509_crt_parse_file(chain, "/etc/pki/tls/certs/ca-bundle.crt");
    } else if (FileUtil::Exists("/usr/share/ssl/certs/ca-bundle.crt")) {
        mbedtls_x509_crt_parse_file(chain, "/usr/share/ssl/certs/ca-bundle.crt");
    } else if (FileUtil::Exists("/usr/local/share/certs/ca-root-nss.crt")) {
        mbedtls_x509_crt_parse_file(chain, "/usr/local/share/certs/ca-root-nss.crt");
    } else if (FileUtil::Exists("/etc/ssl/cert.pem")) {
        mbedtls_x509_crt_parse_file(chain, "/etc/ssl/cert.pem");
    } else if (FileUtil::Exists("/etc/ssl/certs")) {
        mbedtls_x509_crt_parse_path(chain, "/etc/ssl/certs");
    } else {
        mbedtls_x509_crt_free(chain);
        free(chain);
        return nullptr;
    }
#endif

    return chain;
}

} // namespace Common
