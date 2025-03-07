Grammar
=======

The Mocha language grammar is represented using a mix of EBNF and regular
expressions. Syntax parsers can be built using this specification::

        module ::= use-expr
               ::= function-decl
               ::= type-decl
               ::= type-alias
               ::= builtin-call

        use-expr ::= 'use' use-block
                 ::= 'use' use-path

        use-block ::= '{' (use-path)+ '}'
        use-path ::= string
                 ::= symbol ('.' symbol)*

        function-decl ::= 'fn' symbol [function-params] ['->' type] block
        function-params ::= '(' [function-param (',' function-param)*] ')'
        function-param ::= symbol ':' type

        type-decl ::= 'type' symbol '{' (type-field)* '}'
        type-field ::= symbol ':' (type | type-method)
        type-method ::= ['static'] 'fn' [function-params] ['->' type] block

        type-alias ::= 'type' symbol '=' type

        builtin-call ::= symbol '(' [(rvalue | type) (',' (rvalue | type))*] ')'

        type ::= ['&'] symbol ['[' ']']

        statement ::= var-decl
                  ::= var-assign
                  ::= builtin-call
                  ::= condition
                  ::= call
                  ::= member-call
                  ::= ret

        var-decl ::= symbol ':' type ['=' rvalue]
        var-assign ::= lvalue '=' rvalue
        call ::= symbol '(' [rvalue (',' rvalue)*] ')'
        ret ::= 'ret' rvalue

        condition ::= '(' rvalue ')' '?' (block | rvalue) [':' (block | rvalue)]
        block ::= '{' (statement)* '}'

        member-call ::= symbol '.' call

        rvalue ::= '(' rvalue ')'
               ::= literal
               ::= symbol
               ::= deref
               ::= pointer-to
               ::= member
               ::= member-deref
               ::= member-pointer-to
               ::= call
               ::= member-call
               ::= rvalue op rvalue
               ::= tuple

        lvalue ::= symbol
               ::= deref
               ::= member
               ::= member-deref

        deref ::= '*' lvalue
        pointer-to ::= '&' lvalue

        tuple ::= '[' [rvalue (',' rvalue)*] ']'

        member ::= symbol '.' symbol
        member-deref ::= '*' member
        member-pointer-to ::= '&' member

        literal ::= string
                ::= number
                ::= boolean

        symbol ::= /[A-z_]\w*/
        string ::= /('[^']*'|"[^"]*")/
        number ::= /\d+(\.\d+)?/
        boolean ::= 'true'
                ::= 'false'

        op ::= '+'
           ::= '-'
           ::= '*'
           ::= '/'
