# camilascript
the rules are simple, there are keychars:

	;   assign variable
	:   define variable (careful, this will shadow)
	#   represent number
	"   represent string
	!   not operator
	$   variable access, or param access
	'   builtin function call
	()  enclosure, allows operators:
		+-*÷%  arithmetic operators
		=≤≥<>≠ comparison operators
		.      accessor operator
		|&     logical operators
	[]  list builder
	{}  closure
	\   early return a value
	?   conditional (takes condition, if expr, else expr)
	@   run closure, takes arguments as a list (required)
	_   null or nop
	^   infinite loop (can be broken out of)
	~   refer to current scope
	`   for-loop (takes variable and list structure and closure). early return any value to break, early return a null to continue

builtins:
	'print
	'input
	'concat
	'len
	'range
	'sqrt

Here's an example:

```
:point {
	:x $0
	:y $1
	:magnitude {
		'sqrt (($x*$x) + ($y*$y))
	}
	~
}

:p1 @$point[#3#4]
'print @,magnitude$p1[]
```