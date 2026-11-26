#include "gpu.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <TargetConditionals.h>

bool gpu_metal4_supported(void) {
#if TARGET_OS_SIMULATOR
    return false;
#else
    if (@available(iOS 26.0, macOS 26.0, *)) {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        return device != nil && [device supportsFamily:MTLGPUFamilyMetal4];
    }
    return false;
#endif
}

bool gpu_framework_embedded(const char *name) {
    @autoreleasepool {
        NSString *frameworkName = @(name);
        NSString *path = [[NSBundle.mainBundle.privateFrameworksPath
            stringByAppendingPathComponent:[frameworkName stringByAppendingPathExtension:@"framework"]]
            stringByAppendingPathComponent:frameworkName];
        return [NSFileManager.defaultManager fileExistsAtPath:path];
    }
}
