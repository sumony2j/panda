pipeline {
  agent any
  stages {
    stage('setup') {
      steps {
        sh 'sudo apt-get install -y build-essential gcc-multilib pkg-config bison flex libboost-all-dev libpcap-dev  libelf-dev clang llvm libbpf-dev linux-tools-$(uname -r)'
      }
    }

    stage('build') {
      steps {
        sh '''cd src
./configure
make
sudo make install'''
      }
    }

    stage('test') {
      steps {
        sh '''cd src/test/parser
./run-tests.sh'''
      }
    }

  }
}