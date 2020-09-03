// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/err_f.h"

namespace Service::ERR {

enum class FatalErrType : u32 {
    Generic = 0,
    Corrupted = 1,
    CardRemoved = 2,
    Exception = 3,
    ResultFailure = 4,
    Logged = 5,
};

enum class ExceptionType : u32 {
    PrefetchAbort = 0,
    DataAbort = 1,
    Undefined = 2,
    VectorFP = 3,
};

static std::string GetErrType(u8 type_code) {
    switch (static_cast<FatalErrType>(type_code)) {
    case FatalErrType::Generic:
        return "Generic";
    case FatalErrType::Corrupted:
        return "Corrupted";
    case FatalErrType::CardRemoved:
        return "CardRemoved";
    case FatalErrType::Exception:
        return "Exception";
    case FatalErrType::ResultFailure:
        return "ResultFailure";
    case FatalErrType::Logged:
        return "Logged";
    default:
        return "Unknown Error Type";
    }
}

static std::string GetExceptionType(u8 type_code) {
    switch (static_cast<ExceptionType>(type_code)) {
    case ExceptionType::PrefetchAbort:
        return "Prefetch Abort";
    case ExceptionType::DataAbort:
        return "Data Abort";
    case ExceptionType::Undefined:
        return "Undefined Exception";
    case ExceptionType::VectorFP:
        return "Vector Floating Point Exception";
    default:
        return "Unknown Exception Type";
    }
}

static std::string GetCurrentSystemTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream time_stream;
    time_stream << std::put_time(std::localtime(&time), "%Y/%m/%d %H:%M:%S");
    return time_stream.str();
}

static void LogGenericInfo(const ErrInfo& errinfo) {
    LOG_CRITICAL(Service_ERR, "PID: 0x{:08X}", errinfo.common.pid);
    LOG_CRITICAL(Service_ERR, "REV: 0x{:08X}_0x{:08X}", errinfo.common.rev_high,
                 errinfo.common.rev_low);
    LOG_CRITICAL(Service_ERR, "TID: 0x{:08X}_0x{:08X}", errinfo.common.title_id_high,
                 errinfo.common.title_id_low);
    LOG_CRITICAL(Service_ERR, "AID: 0x{:08X}_0x{:08X}", errinfo.common.app_title_id_high,
                 errinfo.common.app_title_id_low);
    LOG_CRITICAL(Service_ERR, "ADR: 0x{:08X}", errinfo.common.pc_address);

    ResultCode result_code{errinfo.common.result_code};
    LOG_CRITICAL(Service_ERR, "RSL: 0x{:08X}", result_code.raw);
    LOG_CRITICAL(Service_ERR, "  Level: {}", static_cast<u32>(result_code.level.Value()));
    LOG_CRITICAL(Service_ERR, "  Summary: {}", static_cast<u32>(result_code.summary.Value()));
    LOG_CRITICAL(Service_ERR, "  Module: {}", static_cast<u32>(result_code.module.Value()));
    LOG_CRITICAL(Service_ERR, "  Desc: {}", static_cast<u32>(result_code.description.Value()));
}

ErrInfo errinfo{};

void ERR_F::ThrowFatalError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 1, 32, 0);

    LOG_CRITICAL(Service_ERR, "Fatal error");
    errinfo = rp.PopRaw<ErrInfo>();
    LOG_CRITICAL(Service_ERR, "Fatal error type: {}", GetErrType(errinfo.common.specifier));
    system.SetStatus(Core::System::ResultStatus::FatalError);

    // Generic Info
    LogGenericInfo(errinfo);

    switch (static_cast<FatalErrType>(errinfo.common.specifier)) {
    case FatalErrType::Generic:
    case FatalErrType::Corrupted:
    case FatalErrType::CardRemoved:
    case FatalErrType::Logged: {
        LOG_CRITICAL(Service_ERR, "Datetime: {}", GetCurrentSystemTime());
        break;
    }
    case FatalErrType::Exception: {
        // Register Info
        LOG_CRITICAL(Service_ERR, "ARM Registers:");
        for (u32 index = 0; index < errinfo.exception.context.arm_regs.size(); ++index) {
            if (index < 13) {
                LOG_DEBUG(Service_ERR, "r{}=0x{:08X}", index,
                          errinfo.exception.context.arm_regs.at(index));
            } else if (index == 13) {
                LOG_CRITICAL(Service_ERR, "SP=0x{:08X}",
                             errinfo.exception.context.arm_regs.at(index));
            } else if (index == 14) {
                LOG_CRITICAL(Service_ERR, "LR=0x{:08X}",
                             errinfo.exception.context.arm_regs.at(index));
            } else if (index == 15) {
                LOG_CRITICAL(Service_ERR, "PC=0x{:08X}",
                             errinfo.exception.context.arm_regs.at(index));
            }
        }
        LOG_CRITICAL(Service_ERR, "CPSR=0x{:08X}", errinfo.exception.context.cpsr);

        // Exception Info
        LOG_CRITICAL(Service_ERR, "EXCEPTION TYPE: {}",
                     GetExceptionType(errinfo.exception.info.type));
        switch (static_cast<ExceptionType>(errinfo.exception.info.type)) {
        case ExceptionType::PrefetchAbort:
            LOG_CRITICAL(Service_ERR, "IFSR: 0x{:08X}", errinfo.exception.info.sr);
            LOG_CRITICAL(Service_ERR, "r15: 0x{:08X}", errinfo.exception.info.ar);
            break;
        case ExceptionType::DataAbort:
            LOG_CRITICAL(Service_ERR, "DFSR: 0x{:08X}", errinfo.exception.info.sr);
            LOG_CRITICAL(Service_ERR, "DFAR: 0x{:08X}", errinfo.exception.info.ar);
            break;
        case ExceptionType::VectorFP:
            LOG_CRITICAL(Service_ERR, "FPEXC: 0x{:08X}", errinfo.exception.info.fpinst);
            LOG_CRITICAL(Service_ERR, "FINST: 0x{:08X}", errinfo.exception.info.fpinst);
            LOG_CRITICAL(Service_ERR, "FINST2: 0x{:08X}", errinfo.exception.info.fpinst2);
            break;
        case ExceptionType::Undefined:
            break; // Not logging exception_info for this case
        }
        LOG_CRITICAL(Service_ERR, "Datetime: {}", GetCurrentSystemTime());
        break;
    }

    case FatalErrType::ResultFailure: {
        // Failure Message
        LOG_CRITICAL(Service_ERR, "Failure Message: {}", errinfo.result_failure.message);
        LOG_CRITICAL(Service_ERR, "Datetime: {}", GetCurrentSystemTime());
        break;
    }

    } // switch FatalErrType

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

ERR_F::ERR_F(Core::System& system) : ServiceFramework("err:f", 1), system(system) {
    static const FunctionInfo functions[] = {
        {0x00010800, &ERR_F::ThrowFatalError, "ThrowFatalError"},
        {0x00020042, nullptr, "SetUserString"},
    };
    RegisterHandlers(functions);
}

ERR_F::~ERR_F() = default;

void InstallInterfaces(Core::System& system) {
    auto errf = std::make_shared<ERR_F>(system);
    errf->InstallAsNamedPort(system.Kernel());
}

} // namespace Service::ERR
