#include "MacDockIcon.h"
#include <juce_core/juce_core.h>
#include <BinaryData.h>

#if JUCE_MAC
#import <AppKit/AppKit.h>

namespace nova::mac {
void setApplicationDockIcon() {
    @autoreleasepool {
        NSImage* image = [NSImage imageNamed:@"NovaSynth"];
        if (image == nil) {
            NSBundle* bundle = [NSBundle mainBundle];
            NSString* icnsPath = [bundle pathForResource:@"NovaSynth" ofType:@"icns"];
            if (icnsPath != nil) {
                image = [[NSImage alloc] initWithContentsOfFile:icnsPath];
            }
        }
        if (image == nil) {
            int dataSize = 0;
            const char* data = NovaResources::getNamedResource("NovaSynth_Icon_256_png", dataSize);
            if (data != nullptr && dataSize > 0) {
                NSData* nsData = [NSData dataWithBytes:data length:static_cast<NSUInteger>(dataSize)];
                image = [[NSImage alloc] initWithData:nsData];
            }
        }
        if (image != nil) {
            [NSApp setApplicationIconImage:image];
        }
    }
}
}
#else
namespace nova::mac {
void setApplicationDockIcon() {}
}
#endif
