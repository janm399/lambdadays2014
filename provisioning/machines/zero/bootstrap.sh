#!/bin/sh

# Chicken & egg problem: when the machine boots, we install Puppet so that we can run through
# the manifests and modules to actually provision the components we need.

sudo rpm -ivh https://yum.puppetlabs.com/el/6/products/x86_64/puppetlabs-release-6-7.noarch.rpm
sudo yum install puppet -y
