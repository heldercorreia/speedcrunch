// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#import <Foundation/Foundation.h>

void* createDockTestAutoreleasePool()
{
    return [[NSAutoreleasePool alloc] init];
}

void drainDockTestAutoreleasePool(void* pool)
{
    [static_cast<NSAutoreleasePool*>(pool) drain];
}
