#!/bin/bash

# Chicken & egg problem: when the machine boots, we install Puppet so that we can run through
# the manifests and modules to actually provision the components we need.

wget https://apt.puppetlabs.com/puppetlabs-release-raring.deb
sudo dpkg -i puppetlabs-release-raring.deb
sudo apt-get update
sudo puppet module install puppetlabs-rabbitmq