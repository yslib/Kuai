if exists("b:current_syntax") | finish | endif

syn keyword tofuKeyword func dim let struct return import
syn keyword tofuType bool char i8 i16 i32 i64 f32 f64
syn match tofuAttribute "@\w\+"
syn match tofuNumber "\<\d\+\(\.\d\+\)\?\>"
syn region tofuString start='"' end='"' skip='\\"'
syn match tofuComment "//.*$"

hi def link tofuKeyword Keyword
hi def link tofuType Type
hi def link tofuAttribute PreProc
hi def link tofuNumber Number
hi def link tofuString String
hi def link tofuComment Comment

let b:current_syntax = "tofu"
