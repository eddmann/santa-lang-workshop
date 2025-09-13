open System
open System.IO
open ElfLang.Lexer

[<EntryPoint>]
let main argv =
    try
        match argv with
        | [| "tokens"; filepath |] ->
            let content = File.ReadAllText(filepath)
            let tokens = tokenize content
            for token in tokens do
                printfn "%s" (formatTokenAsJsonLine token)
            0
        | [| "ast"; filepath |] ->
            let content = File.ReadAllText(filepath)
            let tokens = tokenize content
            let ast = ElfLang.Parser.parse tokens
            let json = ElfLang.Parser.programToJson ast
            let prettyJson = ElfLang.Parser.prettyPrintJson json
            printf "%s" prettyJson
            0
        | [| filepath |] ->
            let content = File.ReadAllText(filepath)
            let tokens = tokenize content
            let ast = ElfLang.Parser.parse tokens
            let result = ElfLang.Evaluator.evaluate ast
            printf "%s" result
            0
        | _ ->
            eprintfn "Usage: %s [tokens|ast] <file>" (Environment.GetCommandLineArgs().[0])
            1
    with
    | ex ->
        eprintfn "Error: %s" ex.Message
        1