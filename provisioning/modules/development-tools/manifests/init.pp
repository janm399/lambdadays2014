# Class that installs & provides minimal configuration for the development tools 
# for the native components

class development-tools {

  package { 'cmake':
    ensure => 'installed'
  }

  package { 'git':
    ensure => 'installed'
  }

  package { 'unzip':
    ensure => 'installed'
  }

}
