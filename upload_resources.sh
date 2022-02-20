#!/bin/bash

set -e

echo "Creating tarball..."

now=$(date +'%Y-%m-%d')
filename=wizard-resources-$now.tar.gz
tar -czf /tmp/$filename resources

# s3cmd put /tmp/$filename s3://wizard-resources/$filename
# s3cmd ls s3://wizard-resources/
