// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#import <GameController/GameController.h>

#include "citra_qt/home/gamepad.h"

namespace Home {

PadState ReadPadState() {
    PadState state;
    @autoreleasepool {
        NSArray* controllers = [GCController controllers];
        for (GCController* controller in controllers) {
            GCExtendedGamepad* pad = [controller extendedGamepad];
            if (pad == nil) {
                continue;
            }
            state.connected = true;
            if (@available(macOS 10.15, *)) {
                NSString* category = [controller productCategory];
                if (category != nil && ([category rangeOfString:@"DualShock"].location != NSNotFound ||
                                        [category rangeOfString:@"DualSense"].location != NSNotFound ||
                                        [category rangeOfString:@"PlayStation"].location != NSNotFound)) {
                    state.layout = 1;
                }
            }
            const float threshold = 0.55f;
            GCControllerDirectionPad* dpad = [pad dpad];
            GCControllerDirectionPad* stick = [pad leftThumbstick];
            state.up = state.up || [[dpad up] isPressed] || [[stick yAxis] value] > threshold;
            state.down = state.down || [[dpad down] isPressed] || [[stick yAxis] value] < -threshold;
            state.left = state.left || [[dpad left] isPressed] || [[stick xAxis] value] < -threshold;
            state.right = state.right || [[dpad right] isPressed] || [[stick xAxis] value] > threshold;
            state.a = state.a || [[pad buttonA] isPressed];
            state.b = state.b || [[pad buttonB] isPressed];
            state.x = state.x || [[pad buttonX] isPressed];
            state.y = state.y || [[pad buttonY] isPressed];
            state.l = state.l || [[pad leftShoulder] isPressed];
            state.r = state.r || [[pad rightShoulder] isPressed];
            state.lt = state.lt || [[pad leftTrigger] isPressed];
            state.rt = state.rt || [[pad rightTrigger] isPressed];
            if (@available(macOS 10.15, *)) {
                state.start = state.start || [[pad buttonMenu] isPressed];
            }
        }
    }
    return state;
}

} // namespace Home
