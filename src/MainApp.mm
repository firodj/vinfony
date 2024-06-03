#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <string>

std::string GetBundleResourcePath(const char * path) {
  std::string res;

  CFStringRef pathRef = CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8);
  CFURLRef appUrlRef = CFBundleCopyResourceURL(CFBundleGetMainBundle(), pathRef, NULL, NULL);

  CFStringRef filePathRef = CFURLCopyPath(appUrlRef);
  const char* filePath = CFStringGetCStringPtr(filePathRef, kCFStringEncodingUTF8);
  res = filePath;

  // Release references
  CFRelease(filePathRef);
  CFRelease(appUrlRef);
  CFRelease(pathRef);

  return res;
}