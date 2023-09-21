mkdir tmp_ext2_cli
bin\scpp.exe ext2_cli/main.c tmp_ext2_cli/cc.i
bin\scc.exe tmp_ext2_cli/cc.i tmp_ext2_cli/cc.asm
bin\asm.exe tmp_ext2_cli/cc.asm ext2_cli.exe tmp_ext2_cli/cc.map
mkdir tmp_ext2_format
bin\scpp.exe ext2_format/main.c tmp_ext2_format/cc.i
bin\scc.exe tmp_ext2_format/cc.i tmp_ext2_format/cc.asm
bin\asm.exe tmp_ext2_format/cc.asm ext2_format.exe tmp_ext2_format/cc.map