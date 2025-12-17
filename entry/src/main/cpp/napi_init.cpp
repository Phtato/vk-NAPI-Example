#include "napi/native_api.h"
#include <stdexcept>
#include <rawfile/raw_file_manager.h>
#include <hilog/log.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <iostream>
#include <dirent.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <fstream>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x1145 // 全局domain宏，标识业务领域
#define LOG_TAG "MY_TAG"  // 全局tag宏，标识模块日志tag

// 在定义 LOG_DOMAIN 和 LOG_TAG 之后包含自定义流缓冲区
#include "include/nativeStreamBuf.h"

// 全局 DeviceBuf 实例
static DeviceBuf deviceBuf;

namespace vulkanNdkEnvInfo {
    std::string ohosPath;  // 使用 std::string 安全存储路径
    constexpr size_t EXPECTED_PARAMS = 1U;
    NativeResourceManager *m_aAssetMgr;
    OHNativeWindow *window;
    bool isDestroy;
} // namespace vulkanNdkEnvInfo

void redirectStreamBuf() {
    std::cout.rdbuf(&deviceBuf); // 重定向 cout 到 hilog
}

napi_value TransferSandboxPath(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1] = {nullptr};
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    // Convert the sandbox path and the contents of the text to be written into C-side variables through the Node-API
    // interface.
    size_t pathSize;
    char pathBuf[256];
    napi_get_value_string_utf8(env, argv[0], pathBuf, sizeof(pathBuf), &pathSize);
    // 将路径存储到全局 std::string 中，确保在后续调用中可以安全访问
    vulkanNdkEnvInfo::ohosPath = std::string(pathBuf, pathSize);
    
    OH_LOG_INFO(LOG_APP, "Sandbox path saved: %{public}s", vulkanNdkEnvInfo::ohosPath.c_str());
    return nullptr;
}

void tujiaming_printFiles(const char *dirPath) {
    DIR *__d = (DIR *)opendir(dirPath);
    if (!__d) {

    } else {
        struct dirent *en;
        while ((en = readdir(__d)) != NULL) {
            // Platform-specific logging: use HiLog on OHOS, fallback to stderr elsewhere
            OH_LOG_ERROR(LOG_APP, "filename %{public}s", en->d_name);
        }
        closedir(__d);
    }
}

// 递归复制rawfile资源文件到沙箱目录
void copyRawFileRecursive(NativeResourceManager *resMgr, const std::string &rawPath, const std::string &destPath) {
    // 打开rawfile目录
    RawDir *rawDir = OH_ResourceManager_OpenRawDir(resMgr, rawPath.c_str());
    if (!rawDir) {
        OH_LOG_ERROR(LOG_APP, "Failed to open raw dir: %{public}s", rawPath.c_str());
        return;
    }

    // 创建目标目录
    mkdir(destPath.c_str(), 0755);
    OH_LOG_INFO(LOG_APP, "Created directory: %{public}s", destPath.c_str());

    // 获取目录下文件数量
    int fileCount = OH_ResourceManager_GetRawFileCount(rawDir);
    OH_LOG_INFO(LOG_APP, "Directory %{public}s has %{public}d items", rawPath.c_str(), fileCount);

    // 遍历所有文件和子目录
    for (int i = 0; i < fileCount; i++) {
        const char *fileName = OH_ResourceManager_GetRawFileName(rawDir, i);
        if (!fileName) {
            continue;
        }

        // 构建完整路径
        std::string rawFilePath = rawPath.empty() ? fileName : rawPath + "/" + fileName;
        std::string destFilePath = destPath + "/" + fileName;

        OH_LOG_INFO(LOG_APP, "Processing: %{public}s", rawFilePath.c_str());

        // 尝试作为目录打开
        RawDir *subDir = OH_ResourceManager_OpenRawDir(resMgr, rawFilePath.c_str());
        if (subDir) {
            // 是目录，递归处理
            OH_LOG_INFO(LOG_APP, "Found subdirectory: %{public}s", rawFilePath.c_str());
            OH_ResourceManager_CloseRawDir(subDir);
            copyRawFileRecursive(resMgr, rawFilePath, destFilePath);
        } else {
            // 是文件，复制内容
            RawFile *rawFile = OH_ResourceManager_OpenRawFile(resMgr, rawFilePath.c_str());
            if (rawFile) {
                // 获取文件大小
                long fileSize = OH_ResourceManager_GetRawFileSize(rawFile);
                OH_LOG_INFO(LOG_APP, "Copying file: %{public}s (size: %{public}ld bytes)", rawFilePath.c_str(), fileSize);

                // 读取文件内容
                std::vector<char> buffer(fileSize);
                int readSize = OH_ResourceManager_ReadRawFile(rawFile, buffer.data(), fileSize);

                if (readSize > 0) {
                    // 写入目标文件
                    std::ofstream outFile(destFilePath, std::ios::binary);
                    if (outFile.is_open()) {
                        outFile.write(buffer.data(), readSize);
                        outFile.close();
                        OH_LOG_INFO(LOG_APP, "Successfully copied to: %{public}s", destFilePath.c_str());
                    } else {
                        OH_LOG_ERROR(LOG_APP, "Failed to create file: %{public}s", destFilePath.c_str());
                    }
                } else {
                    OH_LOG_ERROR(LOG_APP, "Failed to read file: %{public}s", rawFilePath.c_str());
                }

                // 关闭文件
                OH_ResourceManager_CloseRawFile(rawFile);
            } else {
                OH_LOG_ERROR(LOG_APP, "Failed to open raw file: %{public}s", rawFilePath.c_str());
            }
        }
    }

    // 关闭目录
    OH_ResourceManager_CloseRawDir(rawDir);
    OH_LOG_INFO(LOG_APP, "Finished processing directory: %{public}s", rawPath.c_str());
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

    // 递归复制 data 目录下的所有内容到沙箱目录
    if (vulkanNdkEnvInfo::m_aAssetMgr && !vulkanNdkEnvInfo::ohosPath.empty()) {
        std::string destPath = vulkanNdkEnvInfo::ohosPath + "/data";
        OH_LOG_INFO(LOG_APP, "Starting to copy data directory to: %{public}s", destPath.c_str());
        copyRawFileRecursive(vulkanNdkEnvInfo::m_aAssetMgr, "data", destPath);
        OH_LOG_INFO(LOG_APP, "Finished copying data directory");
    } else {
        if (vulkanNdkEnvInfo::ohosPath.empty()) {
            OH_LOG_ERROR(LOG_APP, "Sandbox path not set. Please call transferSandboxPath first.");
        }
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
