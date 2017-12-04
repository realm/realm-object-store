@Library('realm-ci') _

jobWrapper {
    stage('Prepare') {
        node('docker') {
            rlmCheckout scm

            dependencies = readProperties file: 'dependencies.list'
            echo "Version in dependencies.list: ${dependencies.VERSION}"

            gitSha = sh(returnStdout: true, script: 'git rev-parse HEAD').trim().take(8)
        }
    }

    stage('Check') {
        def branches = [:]
        branches['linux-sync'] = buildLinux(buildType: 'Debug', sync: 'ON')
        branches['android-sync'] = buildAndroid(buildType: 'Release', sync: 'OFF')
        branches['macos-release'] = buildMacOS(buildType: 'Release')
        branches['macos-debug'] = buildMacOS(buildType: 'Debug')
        branches['ios-release'] = buildAppleDevice(buildType: 'Release', os: 'ios')
        branches['ios-debug'] = buildAppleDevice(buildType: 'MinSizeDebug', os: 'ios')
        branches['watchos-release'] = buildAppleDevice(buildType: 'Release', os: 'watchos')
        branches['watchos-debug'] = buildAppleDevice(buildType: 'MinSizeDebug', os: 'watchos')
        branches['tvos-release'] = buildAppleDevice(buildType: 'Release', os: 'tvos')
        branches['tvos-debug'] = buildAppleDevice(buildType: 'MinSizeDebug', os: 'tvos')
        parallel branches
    }

    stage('Aggregate') {
        def branches = [:]
        branches['cocoa'] = {
            node('osx') {
                sh 'mkdir realm-objectstore-cocoa'
                for (os in [ 'macos', 'ios', 'watchos', 'tvos' ]) {
                    dir("package-${os}") {
                        unstash "package-${os}-${os == 'macos' ? 'Debug' : 'MinSizeDebug'}"
                        unstash "package-${os}-Release"

                        sh 'for f in *.tar.gz; do tar -xf $f; rm $f; done'
                        dir('lib') {
                            def files = findFiles(glob: '*.a')
                            for (file in files) {
                                def newName = file.name.replaceFirst(/(?:-dbg)*\.a$/, "-${os}\$0")
                                sh "mv ${file.name} ${newName}"
                            }
                        }
                        sh 'cp -R include lib ../realm-objectstore-cocoa/'
                    }
                }
                sh 'rm -rf realm-objectstore-cocoa/lib/cmake realm-objectstore-cocoa/lib/librealmsyncserver*'
                sh "tar -cJvf realm-objectstore-cocoa-${gitSha}.tar.xz realm-objectstore-cocoa"
                archive "realm-objectstore-cocoa-${gitSha}.tar.xz"
            }
        }
        parallel branches
    }
}

def buildLinux(Map args) {
    return {
        node('docker') {
            rlmCheckout scm
            docker.build('gcc7:snapshot', '-f gcc7.Dockerfile .').inside {
                sh """
                    set -e
                    rm -rf build && mkdir build && cd build
                    cmake -D CMAKE_BUILD_TYPE=${args.buildType} -D REALM_ENABLE_SYNC=${args.sync} -G Ninja ..
                    ninja
                    tests/tests
                """
            }
        }
    }
}

def buildAndroid(Map args) {
    return {
        node('docker') {
            rlmCheckout scm
            docker.image('tracer0tong/android-emulator').withRun('-e ARCH=armeabi-v7a') { emulator ->
                docker.build('android:snapshot', '-f android.Dockerfile .').inside("--link ${emulator.id}:emulator") {
                    sh """
                        set -e
                        rm -rf build && mkdir build && cd build
                        cmake -D CMAKE_SYSTEM_NAME=Android -D CMAKE_SYSTEM_VERSION=9 -D CMAKE_ANDROID_ARCH_ABI=armeabi-v7a -D CMAKE_BUILD_TYPE=${args.buildType} -D REALM_ENABLE_SYNC=${args.sync} -G Ninja ..
                        ninja -v
                        adb connect emulator
                        timeout 10m adb wait-for-device
                        adb push tests/tests /data/local/tmp
                        adb shell '/data/local/tmp/tests || echo __ADB_FAIL__' | tee adb.log
                        ! grep __ADB_FAIL__ adb.log                
                    """
                }
            }
        }
    }
}

def buildMacOS(Map args) {
    return {
        node('osx') {
            rlmCheckout scm
            withEnv(['DEVELOPER_DIR=/Applications/Xcode-8.2.app/Contents/Developer/']) {
                sh """
                    mkdir build
                    cd build
                    cmake -D CMAKE_TOOLCHAIN_FILE=../CMake/macos.toolchain.cmake -D CMAKE_BUILD_TYPE=${args.buildType} -G Xcode ..
                    xcodebuild -sdk macosx \\
                               -configuration ${args.buildType} \\
                               -target package \\
                               ONLY_ACTIVE_ARCH=NO
                """
                dir('build') {
                    stash name: "package-macos-${args.buildType}", includes: '*.tar.gz'
                }
            }
        }
    }
}

def buildAppleDevice(Map args) {
    return {
        node('osx') {
            rlmCheckout scm
            withEnv(['DEVELOPER_DIR=/Applications/Xcode-8.2.app/Contents/Developer/']) {
                sh """
                    workflow/cross_compile.sh -t ${args.buildType} -o ${args.os}
                """
            }
            dir("build-${args.os}-${args.buildType}") {
                stash name: "package-${args.os}-${args.buildType}", includes: '*.tar.gz'
            }
        }
    }
}