# Class that installs & provides minimal configuration for RabbitMQ.
# It ensures that Erlang and RabbitMQ packages are installed, and then
# enables the ``rabbitmq_management`` plugin.

class rabbitmq {

  # RabbitMQ class requires Erlang and RabbitMQ. So, as the first step, we install both of them, using 
  # Erlang Solutions's and RabbitMQ's packages
  # Note that we install using ``--nodeps``, because we want to avoid having to install *all* the
  # dependencies that OTP relies on.
  package { 'erlang':
    ensure => 'installed',
    source => 'http://packages.erlang-solutions.com/site/esl/esl-erlang/FLAVOUR_1_general/esl-erlang_16.b.3-2~centos~6_amd64.rpm',
    provider => rpm,
    install_options => ['--nodeps']
  }

  package { 'rabbitmq':
    ensure => 'installed',
    source => 'https://www.rabbitmq.com/releases/rabbitmq-server/v3.2.4/rabbitmq-server-3.2.4-1.noarch.rpm',
    provider => rpm,
    install_options => ['--nodeps'],
    require => Package['erlang']
  }

  # Once installed, we enable the ``rabbitmq_management`` plugin by executing the ``rabbitmq-plugins`` program.
  # Ultimately, this writes to ``/etc/rabbitmq/enabled_plugins``.
  exec { 'rabbitmq-plugins':
    command => 'sudo /usr/sbin/rabbitmq-plugins enable rabbitmq_management',
    require => Package['rabbitmq']
  }

  # We watch the changes to the RabbitMQ configuration; upon change, we notify the ``rabbitmq-server`` service
  # to be restarted.
  file { "/etc/rabbitmq/enabled_plugins":
    notify  => Service['rabbitmq-server'],
    require => Package['rabbitmq']
  }

  # The service (executable using ``/sbin/service rabbitmq-server [start|stop|...]``) is set to be enabled at
  # startup, and depending on the ``rabbitmq`` package.
  service { 'rabbitmq-server':
    ensure => running,
    enable => true,
    require => Package['rabbitmq']
  }

}
