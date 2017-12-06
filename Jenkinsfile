@Library('realm-ci') _

stashes = []

jobWrapper {
    stage('Prepare') {
        node('docker') {
            rlmCheckout scm
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
        node('docker') {
            deleteDir()
            for (stash in stashes) {
                unstash name: stash
            }
            sh '''
                set -e
                mkdir aggregate
                find . -maxdepth 1 -type f -name "*.tar.xz" -exec tar xJf {} -C aggregate \\;
                cd aggregate
                tar cJf realm-aggregate-cocoa.tar.xz -- *
            '''
            dir('aggregate') {
                archiveArtifacts 'realm-aggregate-cocoa.tar.xz'
            }
        }

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
                    ninja Tests
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
                        ninja Tests
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
                               -target package
                               ONLY_ACTIVE_ARCH=NO
                    ./aggregate.sh
                """
                dir('build/aggregate') {
                    def stashName = "macos-${args.buildType}"
                    stash includes: "realm-aggregate-*.tar.xz", name: stashName
                    stashes << stashName
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
                    build_dir=\$(echo build-*)
                    cd \${build_dir}
                    ./aggregate.sh
                """
                def buildDir = sh(returnStdout: true, script: "echo build-*").trim()
                dir("${buildDir}/aggregate") {
                    stashName = "${args.os}-${args.buildType}"
                    stash includes: "realm-aggregate-*.tar.xz", name: stashName
                    stashes << stashName                    
                }
            }
        }
    }
}