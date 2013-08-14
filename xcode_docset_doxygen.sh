#!/bin/bash

#  doxygen_create.sh
#  vdcd
#
#  Created by Lukas Zeller on 19.04.13.
#  Copyright (c) 2013 plan44.ch. All rights reserved.

# Build the doxygen documentation for the project and load the docset into Xcode.

# standard path for doxygen when installed with brew (it's a softlink to the binary in the brew cellar)
DOXYGEN_CMD=/usr/local/bin/doxygen
# source root (recursively scanned)
SOURCE_ROOT=$PROJECT_DIR
# put all docs in a subdir
DOCS_ROOT=$PROJECT_DIR/docs

# use XCode temp dir
TEMP_DIR=$PROJECT_TEMP_DIR

# If the config file doesn’t exist, run ‘doxygen -g $SOURCE_ROOT/doxygen.config’ to
# a get default file.

if ! [ -f $SOURCE_ROOT/doxygen.config ]
then
  echo doxygen config file does not exist
  $DOXYGEN_CMD -g $SOURCE_ROOT/doxygen.config
fi

# create docs root if not exists

if ! [ -d $DOCS_ROOT ]
then
  mkdir -p $DOCS_ROOT
fi


# Append the proper input/output directories and docset info to the config file.
# This works even though values are assigned higher up in the file. Easier than sed.

cp $SOURCE_ROOT/doxygen.config $TEMP_DIR/doxygen.config

OUTPUT_DOCSET=$DOCS_ROOT/DoxygenDocs.docset
DOCSET_BUNDLE_ID=ch.plan44.$PROJECT_NAME.doxygen

echo "INPUT = $SOURCE_ROOT" >> $TEMP_DIR/doxygen.config
echo "OUTPUT_DIRECTORY = $OUTPUT_DOCSET" >> $TEMP_DIR/doxygen.config
# This command makes all work! it tells Doxygen to recursively scan the source folder
echo "RECURSIVE = YES" >> $TEMP_DIR/doxygen.config
#echo "EXTRACT_ALL = YES" >> $TEMP_DIR/doxygen.config
echo "GENERATE_DOCSET = YES" >> $TEMP_DIR/doxygen.config
echo "DOCSET_BUNDLE_ID = $DOCSET_BUNDLE_ID" >> $TEMP_DIR/doxygen.config
echo "EXCLUDE = $DOCS_ROOT" >> $TEMP_DIR/doxygen.config

# Run doxygen on the updated config file.
# Note: doxygen creates a Makefile that does most of the heavy lifting.

$DOXYGEN_CMD $TEMP_DIR/doxygen.config

# make will invoke docsetutil. Take a look at the Makefile to see how this is done.

make -C $OUTPUT_DOCSET/html install
echo "success"

# Construct a temporary applescript file to tell Xcode to load a docset.

rm -f $TEMP_DIR/loadDocSet.scpt

echo "tell application \"Xcode\"" >> $TEMP_DIR/loadDocSet.scpt
echo "load documentation set with path \"$USER_LIBRARY_DIR/Developer/Shared/Documentation/DocSets/$DOCSET_BUNDLE_ID.docset\"" >> $TEMP_DIR/loadDocSet.scpt
echo "end tell" >> $TEMP_DIR/loadDocSet.scpt

# Run the load-docset applescript command.

osascript $TEMP_DIR/loadDocSet.scpt

exit 0

