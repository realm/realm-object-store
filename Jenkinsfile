#!groovy
def getSourceArchive() {
  checkout scm
  sh 'git clean -ffdx -e .????????'
  sh 'git submodule update --init'
}

def buildDockerEnv(name, dockerfile='Dockerfile', extra_args='') {
  docker.withRegistry("https://${env.DOCKER_REGISTRY}", "ecr:eu-west-1:aws-ci-user") {
    sh "sh ./packaging/docker_build.sh $name . ${extra_args}"
  }
  return docker.image(name)
}

//if (env.BRANCH_NAME != 'master') {
env.DOCKER_PUSH = "1"
//}

stage('check') {
  node('docker') {
    getSourceArchive()
    def image = buildDockerEnv("ci/realm-object-store:build")
    image.inside() {
      sh """
        . /opt/rh/devtoolset-3/enable
        cmake -DCMAKE_BUILD_TYPE=Coverage .
        make -j2 generate-coverage-cobertura
      """
    }
  }
}
