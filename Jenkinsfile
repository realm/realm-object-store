#!groovy
def getSourceArchive() {
  checkout scm
  sh 'git clean -ffdx -e .????????'
  sh 'git submodule update --init --recursive'

  git credentialsId: 'realm-ci-ssh', url: 'git@github.com:realm/realm-sync.git'
  dir('realm-sync') {
    sh 'git checkout v1.0.0-BETA-3.3'
  }
}

def readGitTag() {
  sh "git describe --exact-match --tags HEAD | tail -n 1 > tag.txt 2>&1 || true"
  def tag = readFile('tag.txt').trim()
  return tag
}

def readGitSha() {
  sh "git rev-parse HEAD | cut -b1-8 > sha.txt"
  def sha = readFile('sha.txt').readLines().last().trim()
  return sha
}

def buildDockerEnv(name, dockerfile='Dockerfile', extra_args='') {
  docker.withRegistry("https://${env.DOCKER_REGISTRY}", "ecr:eu-west-1:aws-ci-user") {
    sh "sh ./packaging/docker_build.sh $name . ${extra_args}"
  }
  return docker.image(name)
}

if (env.BRANCH_NAME == 'master') {
  env.DOCKER_PUSH = "1"
}

def doBuildLinux() {
  return {
    node('docker') {
      try {
        getSourceArchive()
        def image = buildDockerEnv("ci/realm-object-store:build")
        image.inside() {
          sh """
            . /opt/rh/devtoolset-3/enable
            /source/packaging/build_inside.sh
          """
        }
        currentBuild.result = 'SUCCESS'
      } catch (Exception err) {
        currentBuild.result = 'FAILURE'
      }

      step([
        $class: 'CoberturaPublisher',
        autoUpdateHealth: false,
        autoUpdateStability: false,
        coberturaReportFile: 'coverage.xml',
        failNoReports: true,
        failUnhealthy: false,
        failUnstable: false,
        maxNumberOfBuilds: 0,
        onlyStable: false,
        sourceEncoding: 'ASCII',
        zoomCoverageChart: false
      ])
    }
  }
}

def setBuildName(newBuildName) {
  currentBuild.displayName = "${currentBuild.displayName} - ${newBuildName}"
}

stage('prepare') {
  node('docker') {
    getSourceArchive()

    gitTag = readGitTag()
    gitSha = readGitSha()
    echo "tag: ${gitTag}"
    if (gitTag == "") {
      echo "No tag given for this build"
      setBuildName("${gitSha}")
    } else {
      echo "Building release: '${gitTag}'"
      setBuildName("Tag ${gitTag}")
    }
  }
}

stage('unit-tests') {
  parallel(
    linux: doBuildLinux()
  )
}
