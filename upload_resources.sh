#!/bin/bash

set -e

echo "Creating tarball..."

now=$(date +'%Y-%m-%d')
filename=wizard-resources-$now.tar.gz
tar -czf /tmp/$filename resources
