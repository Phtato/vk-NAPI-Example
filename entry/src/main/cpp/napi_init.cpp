#include "napi/native_api.h"
#include <stdexcept>
#include <rawfile/raw_file_manager.h>
#include <hilog/log.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <iostream>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x1145 // 全局domain宏，标识业务领域
#define LOG_TAG "MY_TAG"  // 全局tag宏，标识模块日志tag

// 在定义 LOG_DOMAIN 和 LOG_TAG 之后包含自定义流缓冲区
#include "include/nativeStreamBuf.h"

// 全局 DeviceBuf 实例
static DeviceBuf deviceBuf;

namespace vulkanNdkEnvInfo {
    inline char *ohosPath;
    constexpr size_t EXPECTED_PARAMS = 1U;
    inline NativeResourceManager *m_aAssetMgr;
    inline OHNativeWindow *window;
    inline bool isDestroy;
} // namespace vulkanNdkEnvInfo

void redirectStreamBuf(){
    std::cout.rdbuf(&deviceBuf);  // 重定向 cout 到 hilog
}

napi_value TransferSandboxPath(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    // Convert the sandbox path and the contents of the text to be written into C-side variables through the Node-API
    // interface.
    size_t pathSize, contentsSize;
    char pathBuf[256];
    napi_get_value_string_utf8(env, argv[0], pathBuf, sizeof(pathBuf), &pathSize);
    // Open the file through the specified path.
    vulkanNdkEnvInfo::ohosPath = pathBuf;
    return nullptr;
}

napi_value createResourceManagerInstance(napi_env env, napi_callback_info info) {
    size_t argc = vulkanNdkEnvInfo::EXPECTED_PARAMS;
    napi_value argv[vulkanNdkEnvInfo::EXPECTED_PARAMS]{};

    auto result = napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (result != napi_ok) {
        throw std::runtime_error("get cb info failed");
        return nullptr;
    }

    if (argc != vulkanNdkEnvInfo::EXPECTED_PARAMS) {
        throw std::runtime_error("Core::OnCreate - Parameters expected: %zu, got: %zu");
        return nullptr;
    }

    if (NativeResourceManager *rm = OH_ResourceManager_InitNativeResourceManager(env, argv[0]); rm) {
        vulkanNdkEnvInfo::m_aAssetMgr = rm;
    }

    return nullptr;
}

// XComponent在创建Surface时的回调函数
void OnSurfaceCreatedCB(OH_NativeXComponent *component, void *window) {
    // 在回调函数里可以拿到OHNativeWindow
    vulkanNdkEnvInfo::window = static_cast<OHNativeWindow *>(window);
}

void OnDestroyCB(OH_NativeXComponent *component, void *window) { vulkanNdkEnvInfo::isDestroy = true; }

OH_NativeXComponent_Callback callback;
EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
    // 重定向 std::cout 到 hilog
    redirectStreamBuf();
    
    napi_property_descriptor desc[] = {
        {"transferSandboxPath", nullptr, &TransferSandboxPath, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sendResourceManagerInstance", nullptr, &createResourceManagerInstance, nullptr, nullptr, nullptr,
         napi_default, nullptr}

    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    
    napi_value exportInstance = nullptr;
    // 用来解析出被wrap了NativeXComponent指针的属性
    napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
    OH_NativeXComponent *nativeXComponent = nullptr;
    // 通过napi_unwrap接口，解析出NativeXComponent的实例指针
    napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&nativeXComponent));
    // 获取XComponentId
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    OH_NativeXComponent_GetXComponentId(nativeXComponent, idStr, &idSize);

    callback.OnSurfaceCreated = OnSurfaceCreatedCB;
    callback.OnSurfaceDestroyed = OnDestroyCB;
    OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);

    return exports;
}
EXTERN_C_END

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = ((void *)0),
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void) { napi_module_register(&demoModule); }
