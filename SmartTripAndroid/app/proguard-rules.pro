# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in /home/ivashov/bin/android-sdk/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Add any project specific keep options here:

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

-dontwarn org.xmlpull.v1.**
-dontwarn sun.misc.Unsafe
-dontwarn org.slf4j.impl.**
-dontwarn com.caverock.**

-include proguard/proguard-butterknife-7.pro
-include proguard/proguard-eventbus.pro
-include proguard/proguard-rx-java.pro
-include proguard/proguard-facebook-stetho.pro