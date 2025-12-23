# Android Build Requirements

## Prerequisites

### Java JDK 17
```bash
sudo apt install openjdk-17-jdk
```

### Android SDK

Download command-line tools:
```bash
wget https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
mkdir -p ~/android-sdk/cmdline-tools
unzip commandlinetools-linux-*.zip -d ~/android-sdk/cmdline-tools
mv ~/android-sdk/cmdline-tools/cmdline-tools ~/android-sdk/cmdline-tools/latest
```

Install required components:
```bash
export ANDROID_HOME=~/android-sdk
export PATH=$PATH:$ANDROID_HOME/cmdline-tools/latest/bin

sdkmanager "platform-tools" "platforms;android-34" "build-tools;34.0.0" "ndk;26.1.10909125"
```

## Environment Variables

Add to your shell profile (~/.bashrc or ~/.zshrc):
```bash
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
export ANDROID_HOME=~/android-sdk
export ANDROID_SDK_ROOT=$ANDROID_HOME
```

## Build

From the project root:
```bash
make android
```

Or from this directory:
```bash
./gradlew assembleDebug
```

## Output

APK location: `app/build/outputs/apk/debug/app-debug.apk`
