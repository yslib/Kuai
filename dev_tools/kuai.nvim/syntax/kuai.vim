if exists("b:current_syntax") | finish | endif

syn keyword kuaiKeyword func dim let struct return import if else while for in break continue
syn keyword kuaiType bool char i8 i16 i32 i64 f32 f64
syn match kuaiAttribute "@\w\+"
syn match kuaiNumber "\<\d\+\(\.\d\+\)\?\>"
syn region kuaiString start='"' end='"' skip='\\"'
syn match kuaiComment "//.*$"

hi def link kuaiKeyword Keyword
hi def link kuaiType Type
hi def link kuaiAttribute PreProc
hi def link kuaiNumber Number
hi def link kuaiString String
hi def link kuaiComment Comment

let b:current_syntax = "kuai"
