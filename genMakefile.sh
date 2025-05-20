#!/bin/bash

function gen_deps() {
  local inc_prefix="$1"
  local src_prefix="$2"
  local outputfile="$3"
  local objsfile="$4"
  local command="$5"
  for file in $(find $src_prefix -type f); do
    local file_noprefix=$(echo $file | sed -E 's@^'"$src_prefix"'@@')
    local objfile='$(OBJ_DIR)/'$(echo $file_noprefix | sed -E 's/\.c$/.o/; s@/@-@g')
    local headers=$(gcc -MM "$file" -I$inc_prefix | sed -E -e 's/^.*:.*\.c\>//; s/\\//g' -e 's@'" $inc_prefix"'@ $(INC_DIR)/@g')
    echo $objfile ':' '$(SRC_DIR)/'$file_noprefix $headers '|' create_dir >> $outputfile
    echo -e "\t$command\n" >> $outputfile
    echo $objfile \\ >> $objsfile
  done
}

echo "" > deps.mk
echo "OBJECTS = \\" > objects.mk

gen_deps include/ src/ deps.mk objects.mk '$(CC) $(CFLAGS) -c -o $@ $<'
exit 0
