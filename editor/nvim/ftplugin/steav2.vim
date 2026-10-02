" Filetype plugin for steav2 (.sts)

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

setlocal commentstring=//\ %s comments=://
let b:undo_ftplugin = "setlocal commentstring< comments<"
