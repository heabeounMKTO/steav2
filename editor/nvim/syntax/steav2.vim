" Vim syntax file
" Language: steav2 (.sts)

if exists("b:current_syntax")
  finish
endif

syn case match

syn keyword steav2BongSlanhOun bongSlanhOun
syn keyword steav2Include      yok jea
syn keyword steav2Conditional  ber minjengte
syn keyword steav2Repeat       somhab nvpeldae
syn keyword steav2Keyword      akthe rupamun morvenh nis super
syn keyword steav2Structure    sampoan tnak
syn keyword steav2Statement    jongyeytha
syn keyword steav2Operator     ng reu
syn keyword steav2Boolean      ok ort
syn keyword steav2Constant     sone
syn keyword steav2Type         Lek LekKut LekThom LekThomKlang Ahsor Boolean OrtMean
syn keyword steav2Type         Vec2 Vec3 Vec4
syn keyword steav2Special      init

" Keywords above take priority, so `ber (` stays a conditional.
syn match   steav2Function     "\<\h\w*\ze\s*("
" rupamun +(a: Vec3, b: Vec3), operator overloads
syn match   steav2Function     "\(\<rupamun\s\+\)\@<=\(==\|!=\|<=\|>=\|[-+*/<>]\)"
" the type after `:` / `->`, and optionals / arrays around it
syn match   steav2Type         "\(\(:\|->\)\s*\[*\)\@<=\u\w*"
syn match   steav2Number       "\<\d\+\(\.\d\+\)\=\>"
" No escape sequences in the language; strings may span lines.
syn region  steav2String       start=+"+ end=+"+
syn keyword steav2Todo         contained TODO FIXME XXX
syn match   steav2Comment      "//.*$" contains=steav2Todo

hi def link steav2BongSlanhOun PreProc
hi def link steav2Include      Include
hi def link steav2Conditional  Conditional
hi def link steav2Repeat       Repeat
hi def link steav2Keyword      Keyword
hi def link steav2Structure    Structure
hi def link steav2Statement    Statement
hi def link steav2Operator     Operator
hi def link steav2Boolean      Boolean
hi def link steav2Constant     Constant
hi def link steav2Type         Type
hi def link steav2Special      Special
hi def link steav2Function     Function
hi def link steav2Number       Number
hi def link steav2String       String
hi def link steav2Todo         Todo
hi def link steav2Comment      Comment

let b:current_syntax = "steav2"
