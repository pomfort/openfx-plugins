// Copyright (c) 2026 Pomfort GmbH
// SPDX-License-Identifier: BSD-3-Clause

#import <AppKit/AppKit.h>

// Opens a webpage in the user's default browser. The plugin declares which page
// through kPluginSupportWebpage; this function carries no plugin-specific knowledge.
void OpenWebpage(const char* p_URL)
{
    if (p_URL == nullptr)
    {
        return;
    }

    NSString* urlString = [NSString stringWithUTF8String:p_URL];
    NSURL* url = [NSURL URLWithString:urlString];

    if (url == nil)
    {
        return;
    }

    [[NSWorkspace sharedWorkspace] openURL:url];
}
