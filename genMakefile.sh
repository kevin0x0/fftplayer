#!/bin/bash

function gen_deps() {
  local incdir="$1"
  local dir="$2"
  local unique_id="$3"
  local outputfile="$4"
  local objsfile="$5"
  local command="$6"
  for file in $dir*; do
    if [ -f $file ]; then
      local list=$(gcc -MM $file $incdir | tr ': \\' '\n')
      local target='$(OBJ_DIR)/l'${unique_id}_$(echo $list | tr ' ' '\n' | awk '/\.o/{print $1}')
      unique_id=$(expr $unique_id + 1)
      local source='$(SRC_DIR)/'$(echo $list | tr ' ' '\n' | awk '/\.c/{sub(/src\//, ""); print $1}')
      local headers=
      for header in $(echo $list | tr ': \\' '\n' | awk '/^include\/.*\.h/{sub(/include\//, "$(INC_DIR)/"); print $1}'); do
        headers="$headers "$header
      done
      echo $target ':' $source $headers '|' create_dir >> $outputfile
      echo -e "\t$command\n" >> $outputfile
      echo $target \\ >> $objsfile
    else
      gen_deps "$incdir" $file/ "$unique_id" $outputfile $objsfile "$command"
      unique_id=$?
    fi
  done
  return $unique_id
}

echo "" > deps.mk
echo "OBJECTS = \\" > objects.mk

gen_deps '-I ./include' ./src/ 0 deps.mk objects.mk '$(CC) $(CFLAGS) -c -o $@ $<'
exit 0
