{-# LANGUAGE OverloadedStrings #-}
module Main where

import System.Environment (getArgs)
import System.Exit (exitFailure, exitSuccess)
import System.IO (IOMode(ReadMode), withFile, hSetEncoding, utf8, hGetContents, stdout)
import Control.Exception (evaluate)
import qualified Data.Char as C
import Data.Aeson (ToJSON(..), (.=), Value(..), object)
import qualified Data.Aeson as A
import qualified Data.Aeson.Encode.Pretty as P
import qualified Data.ByteString.Lazy.Char8 as BL
import qualified Data.ByteString as BS
import qualified Data.Text as T
import qualified Data.Text.Encoding as TE
import qualified Data.Map.Strict as M
import qualified Data.Set as S
import Numeric (showFFloat)
import Data.IORef (IORef, newIORef, readIORef, writeIORef)

-- Tokens used for both printing and parsing
data Tok = Tok { ttype :: String, tvalue :: String }

-- AST data structures
data Program = Program { pStatements :: [Statement] } deriving (Eq, Show)
data Statement
  = SExpression Expr
  | SComment String
  deriving (Eq, Show)

data Expr
  = EIdentifier String
  | EInteger String
  | EDecimal String
  | EString String
  | EBoolean Bool
  | ENil
  | ELet { eMut :: Bool, eName :: String, eValue :: Expr }
  | EAssign { aName :: String, aValue :: Expr }
  | EInfix { iLeft :: Expr, iOp :: String, iRight :: Expr }
  | EPrefix { pOp :: String, pOperand :: Expr }
  | EList [Expr]
  | ESet [Expr]
  | EDict [(Expr, Expr)]
  | EIndex { idxLeft :: Expr, idxIndex :: Expr }
  | EIf { ifCond :: Expr, ifCons :: Block, ifAlt :: Block }
  | EFunction { fParams :: [String], fBody :: Block }
  | ECall { cFunc :: Expr, cArgs :: [Expr] }
  | ECompose [Expr]
  | EThread { tInitial :: Expr, tFunctions :: [Expr] }
  deriving (Eq, Show)

newtype Block = Block { bStatements :: [Statement] } deriving (Eq, Show)

main :: IO ()
main = do
  hSetEncoding stdout utf8
  args <- getArgs
  case args of
    ["tokens", file] -> do
      src <- readUtf8 file
      mapM_ (putStrLn . renderTok) (lexElf src)
    ["ast", file] -> do
      src <- readUtf8 file
      let toks = lexElf src ++ [Tok "EOF" ""]
      let ast = parseProgram toks
      BL.putStrLn (P.encodePretty' (P.defConfig { P.confIndent = P.Spaces 2 }) (toJSON ast))
    [file] -> do
      src <- readUtf8 file
      let toks = lexElf src ++ [Tok "EOF" ""]
      let ast = parseProgram toks
      runProgram ast
    _ -> do
      putStrLn "Usage: elf [tokens|ast] <file> | elf <file>"
      exitFailure

-- Rendering: tokens
renderTok :: Tok -> String
renderTok (Tok ty val) =
  '{' : "\"type\":\"" ++ ty ++ "\",\"value\":\"" ++ jsonEsc val ++ "\"}"

jsonEsc :: String -> String
jsonEsc = concatMap esc
  where
    esc '"'  = "\\\""
    esc '\\' = "\\\\"
    esc '\n' = "\\n"
    esc '\t' = "\\t"
    esc c     = [c]

readUtf8 :: FilePath -> IO String
readUtf8 fp = withFile fp ReadMode $ \h -> do
  hSetEncoding h utf8
  s <- hGetContents h
  _ <- evaluate (length s)
  pure s

-- Lexer (stage 1 with a few more symbols)
lexElf :: String -> [Tok]
lexElf = go
  where
    go [] = []
    go (c:cs)
      | C.isSpace c = go cs
      | c == '/' && isPrefix "//" (c:cs) = let (com, rest) = spanUntilNewline (c:cs)
                                            in Tok "CMT" com : go rest
      | isPrefix "#{" (c:cs) = Tok "#{" "#{" : go (drop 2 (c:cs))
      | isPrefix "==" (c:cs) = Tok "==" "==" : go (drop 2 (c:cs))
      | isPrefix "!=" (c:cs) = Tok "!=" "!=" : go (drop 2 (c:cs))
      | isPrefix ">=" (c:cs) = Tok ">=" ">=" : go (drop 2 (c:cs))
      | isPrefix "<=" (c:cs) = Tok "<=" "<=" : go (drop 2 (c:cs))
      | isPrefix "&&" (c:cs) = Tok "&&" "&&" : go (drop 2 (c:cs))
      | isPrefix "||" (c:cs) = Tok "||" "||" : go (drop 2 (c:cs))
      | isPrefix "|>" (c:cs) = Tok "|>" "|>" : go (drop 2 (c:cs))
      | isPrefix ">>" (c:cs) = Tok ">>" ">>" : go (drop 2 (c:cs))
      | c == '|' = Tok "|" "|" : go cs
      | c == '"' = let (s, rest) = lexString (c:cs) in Tok "STR" s : go rest
      | isIdentStart c = let (ident, rest) = spanIdent (c:cs)
                             ty = keywordType ident
                         in Tok ty ident : go rest
      | C.isDigit c = let (num, rest) = lexNumber (c:cs) in Tok (numType num) num : go rest
      | c `elem` ("+-*/=;{}[]<>() ,:" :: String) = Tok [c] [c] : go cs
      | otherwise = go cs -- skip unknowns

    isPrefix p s = take (length p) s == p

    spanUntilNewline s = let (h, t) = span (/= '\n') s in (h, dropWhile (== '\n') t)

    isIdentStart ch = C.isAlpha ch || ch == '_'
    isIdentChar ch = C.isAlphaNum ch || ch == '_'
    spanIdent xs = let (a, b) = span isIdentChar xs in (a, b)

    keywordType w = case w of
      "let"  -> "LET"
      "mut"  -> "MUT"
      "if"   -> "IF"
      "else" -> "ELSE"
      "true" -> "TRUE"
      "false"-> "FALSE"
      "nil"  -> "NIL"
      _      -> "ID"

    -- Lex numbers supporting underscores and optional decimal part
    lexNumber xs =
      let (intPart, rest) = span isNumChar xs
      in case rest of
           ('.':r1) | not (null r1) ->
             let (fracPart, r2) = span isNumChar r1
             in (intPart ++ "." ++ fracPart, r2)
           _ -> (intPart, rest)

    isNumChar ch = C.isDigit ch || ch == '_'

    numType s = if '.' `elem` s then "DEC" else "INT"

    -- Lex a double-quoted string, preserving escapes and quotes
    lexString ('"':rest) =
      let (body, rem') = goStr rest
      in ('"' : body ++ ['"'], rem')
    lexString xs = ([head xs], tail xs)

    goStr [] = ([], [])
    goStr ('\\':x:xs) = let (b, r) = goStr xs in ('\\':x:b, r)
    goStr ('"':xs) = ([], xs)
    goStr (x:xs) = let (b, r) = goStr xs in (x:b, r)

-- Parser
data PState = PState { toks :: [Tok], pos :: Int }

parseProgram :: [Tok] -> Program
parseProgram ts = Program { pStatements = loop (PState ts 0) }
  where
    loop st
      | ttype (cur st) == "EOF" = []
      | ttype (cur st) == "CMT" =
          let c = cur st
              st' = advance st
              st'' = optionalSemi st'
          in SComment (tvalue c) : loop st''
      | otherwise =
          let (e, st1) = parseExpression precLowest st
              st2 = optionalSemi st1
          in SExpression e : loop st2

    optionalSemi st'
      | ttype (cur st') == ";" = advance st'
      | otherwise = st'

cur :: PState -> Tok
cur st = let i = pos st in if i < length (toks st) then (toks st) !! i else Tok "EOF" ""

advance :: PState -> PState
advance st = st { pos = pos st + 1 }

match :: String -> PState -> (Bool, PState)
match ty st = if ttype (cur st) == ty then (True, advance st) else (False, st)

expect :: String -> PState -> (Tok, PState)
expect ty st = let t = cur st in if ttype t == ty then (t, advance st) else (t, advance st) -- simple panic-free

-- Precedences
precLowest, precOr, precAnd, precCompare, precThread, precCompose, precAdd, precMul, precCallIndex :: Int
precLowest = 0
precOr = 1
precAnd = 2
precCompare = 3
precThread = 4
precCompose = 5
precAdd = 6
precMul = 7
precCallIndex = 8

precedence :: String -> Int
precedence op = case op of
  "||" -> precOr
  "&&" -> precAnd
  "==" -> precCompare
  "!=" -> precCompare
  ">"  -> precCompare
  "<"  -> precCompare
  ">=" -> precCompare
  "<=" -> precCompare
  "|>" -> precThread
  ">>" -> precCompose
  "+"  -> precAdd
  "-"  -> precAdd
  "*"  -> precMul
  "/"  -> precMul
  _     -> precLowest

parseExpression :: Int -> PState -> (Expr, PState)
parseExpression minPrec st0 =
  let (left0, st1) = parsePrefix st0 in
  loop left0 st1
  where
    loop left st =
      let t = cur st in
      case ttype t of
        "=" -> case left of
                  EIdentifier name ->
                    let st' = advance st
                        (rhs, st'') = parseExpression precLowest st'
                    in loop (EAssign name rhs) st''
                  _ -> (left, st)
        "(" -> -- call
          let st' = advance st
              (args, st'') = parseArgs st'
              left' = ECall left args
          in loop left' st''
        "[" ->
          let st' = advance st
              (idx, st'') = parseExpression precLowest st'
              (_, st''') = expect "]" st''
              left' = EIndex left idx
          in loop left' st'''
        op | op `elem` ["+","-","*","/","==","!=","<",">","<=",
                         ">=", ">>", "|>", "&&", "||"] ->
             let pPrec = precedence op
                 rightAssoc = (op == ">>")
             in if pPrec < minPrec then (left, st) else
               let st' = advance st
                   nextMin = if rightAssoc then pPrec else pPrec + 1
                   (right, st'') = parseExpression nextMin st'
               in case op of
                    ">>" ->
                      let funcsL = case left of
                                      ECompose fs -> fs
                                      _ -> [left]
                          funcsR = case right of
                                      ECompose fs -> fs
                                      _ -> [right]
                      in loop (ECompose (funcsL ++ funcsR)) st''
                    "|>" ->
                      let (initVal, funcsL) = case left of
                                                EThread init fs -> (init, fs)
                                                _ -> (left, [])
                          funcs' = funcsL ++ [right]
                      in loop (EThread initVal funcs') st''
                    _ -> loop (EInfix left op right) st''
        _ -> (left, st)

    parseArgs st =
      case ttype (cur st) of
        ")" -> ([], advance st)
        _ -> let (first, st1) = parseExpression precLowest st
                 (rest, st2) = more st1
             in (first:rest, st2)
    more st = case ttype (cur st) of
                ")" -> ([], advance st)
                "," -> let st' = advance st
                           (e, st'') = parseExpression precLowest st'
                           (es, st''') = more st''
                       in (e:es, st''')
                _ -> ([], st)

parsePrefix :: PState -> (Expr, PState)
parsePrefix st =
  let t = cur st in
  case ttype t of
    "-" -> let st' = advance st; (e, st'') = parseExpression precMul st' in (EPrefix "-" e, st'')
    "INT" -> (EInteger (tvalue t), advance st)
    "DEC" -> (EDecimal (tvalue t), advance st)
    "STR" -> (EString (unquote (tvalue t)), advance st)
    "TRUE" -> (EBoolean True, advance st)
    "FALSE" -> (EBoolean False, advance st)
    "NIL" -> (ENil, advance st)
    "ID" -> (EIdentifier (tvalue t), advance st)
    "[" -> let st' = advance st
               (items, st'') = parseListItems "]" st'
           in (EList items, st'')
    "{" -> let st' = advance st
               (items, st'') = parseListItems "}" st'
           in (ESet items, st'')
    "#{" -> let st' = advance st
                (pairs, st'') = parseDictItems st'
             in (EDict pairs, st'')
    "(" -> let st' = advance st
               (e, st'') = parseExpression precLowest st'
               (_, st''') = expect ")" st''
           in (e, st''')
    "|" -> parseFunction st
    "||" -> parseFunction st
    "LET" -> parseLet st
    "IF" -> parseIf st
    _ -> (EIdentifier (tvalue t), advance st)
  where
    parseListItems end st =
      if ttype (cur st) == end then ([], advance st)
      else let (first, st1) = parseExpression precLowest st
           in collect [first] st1
      where
        collect acc st' =
          case ttype (cur st') of
            ty' | ty' == end -> (reverse acc, advance st')
            "," -> let st'' = advance st'
                       (e, st3) = parseExpression precLowest st''
                   in collect (e:acc) st3
            _ -> (reverse acc, st')

    parseDictItems st =
      case ttype (cur st) of
        "}" -> ([], advance st)
        _ -> let (k, st1) = parseExpression precLowest st
                 (_, st2) = expect ":" st1
                 (v, st3) = parseExpression precLowest st2
                 (rest, st4) = case ttype (cur st3) of
                                  "}" -> ([], advance st3)
                                  "," -> let st' = advance st3
                                             (xs, st'') = parseDictItems st'
                                         in (xs, st'')
                                  _ -> ([], st3)
             in ((k,v):rest, st4)

    parseFunction st =
      -- Already at '|' or '||'
      let st1 = advance st in
      let (params, st2) =
            if ttype (cur st) == "||"
              then ([], advance st) -- consumed both
              else if ttype (cur st1) == "|"
                     then ([], advance st1)
                     else collectParams st1 []
          in
      let (body, st3) = if ttype (cur st2) == "{"
                          then let (b, stB) = parseBlock st2 in (b, stB)
                          else let (e, stE) = parseExpression precLowest st2
                                   b = Block [SExpression e]
                               in (b, stE)
      in (EFunction params body, st3)

    collectParams st ps =
      case ttype (cur st) of
        "|" -> (ps, advance st)
        _ -> let (idTok, st1) = expect "ID" st
                 ps' = ps ++ [tvalue idTok]
             in case ttype (cur st1) of
                  "," -> collectParams (advance st1) ps'
                  _    -> collectParams st1 ps'

    parseLet st =
      let st1 = advance st
          (isMut, st2) = case ttype (cur st1) of
                           "MUT" -> (True, advance st1)
                           _      -> (False, st1)
          (nameTok, st3) = expect "ID" st2
          (_, st4) = expect "=" st3
          (val, st5) = parseExpression precLowest st4
      in (ELet isMut (tvalue nameTok) val, st5)

    parseIf st =
      let st1 = advance st
          (cond, st2) = parseExpression precCompare st1
          (cons, st3) = parseBlock st2
          (_, st4) = expect "ELSE" st3
          (alt, st5) = parseBlock st4
      in (EIf cond cons alt, st5)

parseBlock :: PState -> (Block, PState)
parseBlock st0 =
  let (_, st1) = expect "{" st0 in
  let (ss, st2) = go st1 [] in (Block ss, st2)
  where
    go st acc =
      case ttype (cur st) of
        "}" -> (acc, advance st)
        "EOF" -> (acc, st)
        "CMT" -> let c = tvalue (cur st); st' = advance st; st'' = if ttype (cur st') == ";" then advance st' else st' in go st'' (acc ++ [SComment c])
        _ -> let (e, st1) = parseExpression precLowest st
                 st2 = if ttype (cur st1) == ";" then advance st1 else st1
             in go st2 (acc ++ [SExpression e])

-- String unquoting: remove surrounding quotes and simple escapes
unquote :: String -> String
unquote s =
  let s' = if length s >= 2 && head s == '"' && last s == '"' then init (tail s) else s in
  go s'
  where
    go [] = []
    go ('\\':x:xs) =
      let c = case x of
                'n' -> '\n'
                't' -> '\t'
                '"' -> '"'
                '\\' -> '\\'
                _ -> x
      in c : go xs
    go (x:xs) = x : go xs

-- JSON encoding
instance ToJSON Program where
  toJSON (Program ss) = object ["statements" .= ss, "type" .= ("Program" :: String)]

instance ToJSON Statement where
  toJSON (SExpression e) = object ["type" .= ("Expression" :: String), "value" .= e]
  toJSON (SComment c)    = object ["type" .= ("Comment" :: String),   "value" .= c]

instance ToJSON Block where
  toJSON (Block ss) = object ["statements" .= ss, "type" .= ("Block" :: String)]

instance ToJSON Expr where
  toJSON (EIdentifier n) = object ["name" .= n, "type" .= ("Identifier" :: String)]
  toJSON (EInteger v)    = object ["type" .= ("Integer" :: String), "value" .= v]
  toJSON (EDecimal v)    = object ["type" .= ("Decimal" :: String), "value" .= v]
  toJSON (EString v)     = object ["type" .= ("String"  :: String), "value" .= v]
  toJSON (EBoolean b)    = object ["type" .= ("Boolean" :: String), "value" .= b]
  toJSON ENil            = object ["type" .= ("Nil"     :: String)]
  toJSON (ELet m n v)    = object ["name" .= EIdentifier n, "type" .= (if m then "MutableLet" else "Let" :: String), "value" .= v]
  toJSON (EAssign n v)   = object ["name" .= EIdentifier n, "type" .= ("Assignment" :: String), "value" .= v]
  toJSON (EInfix l op r) = object ["left" .= l, "operator" .= op, "right" .= r, "type" .= ("Infix" :: String)]
  toJSON (EPrefix op x)  = object ["operator" .= op, "operand" .= x, "type" .= ("Prefix" :: String)]
  toJSON (EList xs)      = object ["items" .= xs, "type" .= ("List" :: String)]
  toJSON (ESet xs)       = object ["items" .= xs, "type" .= ("Set"  :: String)]
  toJSON (EDict kvs)     = object ["items" .= map (\(k,v) -> object ["key" .= k, "value" .= v]) kvs, "type" .= ("Dictionary" :: String)]
  toJSON (EIndex l i)    = object ["index" .= i, "left" .= l, "type" .= ("Index" :: String)]
  toJSON (EIf c s a)     = object ["alternative" .= a, "condition" .= c, "consequence" .= s, "type" .= ("If" :: String)]
  toJSON (EFunction ps b)= object ["body" .= b, "parameters" .= map EIdentifier ps, "type" .= ("Function" :: String)]
  toJSON (ECall f as)    = object ["arguments" .= as, "function" .= f, "type" .= ("Call" :: String)]
  toJSON (ECompose fs)   = object ["functions" .= fs, "type" .= ("FunctionComposition" :: String)]
  toJSON (EThread i fs)  = object ["functions" .= fs, "initial" .= i, "type" .= ("FunctionThread" :: String)]

-- =====================
-- Evaluator (Stage 3)
-- =====================

data RVal
  = VInt Integer
  | VDec Double
  | VStr String
  | VBool Bool
  | VNil
  | VBuiltin String -- placeholders for operator functions and puts
  | VClosure (IORef Env) [String] Block
  | VPartial String [RVal]
  | VComposed [RVal]
  | VList [RVal]
  | VSet (S.Set RVal)
  | VDict (M.Map RVal RVal)
  

data Binding = Binding { bMut :: Bool, bVal :: RVal }
type Env = M.Map String Binding

emptyEnv :: Env
emptyEnv = M.fromList
  [ ("puts", Binding { bMut = False, bVal = VBuiltin "puts" })
  , ("+",    Binding { bMut = False, bVal = VBuiltin "+" })
  , ("-",    Binding { bMut = False, bVal = VBuiltin "-" })
  , ("*",    Binding { bMut = False, bVal = VBuiltin "*" })
  , ("/",    Binding { bMut = False, bVal = VBuiltin "/" })
  , ("push", Binding { bMut = False, bVal = VBuiltin "push" })
  , ("assoc",Binding { bMut = False, bVal = VBuiltin "assoc" })
  , ("first",Binding { bMut = False, bVal = VBuiltin "first" })
  , ("rest", Binding { bMut = False, bVal = VBuiltin "rest" })
  , ("size", Binding { bMut = False, bVal = VBuiltin "size" })
  , ("map",  Binding { bMut = False, bVal = VBuiltin "map" })
  , ("filter", Binding { bMut = False, bVal = VBuiltin "filter" })
  , ("fold", Binding { bMut = False, bVal = VBuiltin "fold" })
  ]

runProgram :: Program -> IO ()
runProgram prog = do
  (env', lastVal) <- evalProgram emptyEnv prog
  putStrLn (repr lastVal ++ " ")
  exitSuccess

evalProgram :: Env -> Program -> IO (Env, RVal)
evalProgram env (Program ss) = go env VNil ss
  where
    go e v [] = pure (e, v)
    go e v (SComment _ : xs) = go e v xs
    go e _ (SExpression ex : xs) = do
      (e', v') <- evalExpr e ex
      go e' v' xs

evalExpr :: Env -> Expr -> IO (Env, RVal)
evalExpr env ex = case ex of
  EInteger s -> pure (env, VInt (parseInt s))
  EDecimal s -> pure (env, VDec (parseDec s))
  EString s  -> pure (env, VStr s)
  EBoolean b -> pure (env, VBool b)
  ENil       -> pure (env, VNil)
  EIdentifier n -> case M.lookup n env of
    Just b -> pure (env, bVal b)
    Nothing -> runtimeError ("Identifier can not be found: " ++ n)
  ELet m n v -> do
    (_, vv) <- evalExpr env v
    -- Support recursion: if binding a function, insert self into its captured env
    case vv of
      VClosure ref ps b -> do
        captured <- readIORef ref
        let self = VClosure ref ps b
        writeIORef ref (M.insert n (Binding False self) captured)
        let env' = M.insert n (Binding m self) env
        pure (env', self)
      _ -> do
        let env' = M.insert n (Binding m vv) env
        pure (env', vv)
  EAssign n v -> case M.lookup n env of
    Nothing -> runtimeError ("Identifier can not be found: " ++ n)
    Just (Binding False _) -> runtimeError ("Variable '" ++ n ++ "' is not mutable")
    Just (Binding True _) -> do
      (_, vv) <- evalExpr env v
      let env' = M.insert n (Binding True vv) env
      pure (env', vv)
  EInfix l op r -> do
    (_, lv) <- evalExpr env l
    (_, rv) <- evalExpr env r
    v <- evalInfix op lv rv
    pure (env, v)
  EPrefix op x -> do
    (_, xv) <- evalExpr env x
    case (op, xv) of
      ("-", VInt i) -> pure (env, VInt (-i))
      ("-", VDec d) -> pure (env, VDec (-d))
      _ -> typeErrorUnary op xv
  EList xs -> do
    vals <- mapM (fmap snd . evalExpr env) xs
    pure (env, VList vals)
  ESet xs  -> do
    -- Error if a dictionary literal is directly inside a set literal
    let hasDictLiteral = any isDictLiteral xs
    if hasDictLiteral
      then runtimeError "Unable to include a Dictionary within a Set"
      else do
        vals <- mapM (fmap snd . evalExpr env) xs
        pure (env, VSet (S.fromList vals))
  EDict kvs -> do
    pairs <- mapM (\(k,v) -> do
                       (_, kv') <- evalExpr env k
                       (_, vv') <- evalExpr env v
                       case kv' of
                         VDict _ -> runtimeError "Unable to use a Dictionary as a Dictionary key"
                         _       -> pure (kv', vv')) kvs
    pure (env, VDict (M.fromList pairs))
  EIndex l i -> do
    (_, lv) <- evalExpr env l
    (_, iv) <- evalExpr env i
    v <- evalIndex lv iv
    pure (env, v)
  EIf c t e -> do
    (_, cv) <- evalExpr env c
    if truthy cv
      then evalBlock env t
      else evalBlock env e
  EFunction ps b -> do
    ref <- newIORef env
    pure (env, VClosure ref ps b)
  ECall fn args -> do
    (env1, fval) <- evalExpr env fn
    (env2, argVals) <- evalArgs env1 args
    case fval of
      VBuiltin _  -> apply env2 fval argVals
      VPartial{}  -> apply env2 fval argVals
      VClosure{}  -> apply env2 fval argVals
      VComposed{} -> apply env2 fval argVals
      _           -> runtimeError ("Expected a Function, found: " ++ vType fval)
  ECompose fs -> do
    -- Evaluate functions now and store them in composition
    funs <- mapM (fmap snd . evalExpr env) fs
    pure (env, VComposed funs)
  EThread initV funExprs -> do
    (_, v0) <- evalExpr env initV
    v <- threadApply env v0 funExprs
    pure (env, v)

evalBlock :: Env -> Block -> IO (Env, RVal)
evalBlock env (Block ss) = evalProgram env (Program ss)

apply :: Env -> RVal -> [RVal] -> IO (Env, RVal)
apply env (VBuiltin name) args = applyBuiltin env name [] args
apply env (VPartial name caps) args = applyBuiltin env name caps args
apply env (VClosure ref ps body) args = do
  captured <- readIORef ref
  let (given, _) = splitAt (length ps) args
  if length given < length ps
    then do
      let (boundParams, restParams) = splitAt (length given) ps
          paramBindings = M.fromList [ (p, Binding False v) | (p, v) <- zip boundParams given ]
      ref' <- newIORef (M.union paramBindings captured)
      pure (env, VClosure ref' restParams body)
    else do
      let boundParams = take (length ps) ps
          paramBindings = M.fromList [ (p, Binding False v) | (p, v) <- zip boundParams given ]
          oldKeys = M.keysSet captured
          execEnv = M.union paramBindings captured
      (execEnv', result) <- evalBlock execEnv body
      let newCaptured = M.restrictKeys execEnv' oldKeys
      writeIORef ref newCaptured
      -- also propagate to outer env where variables exist (by reference semantics)
      let envUpdated = foldl (\e k ->
                                case (M.lookup k e, M.lookup k execEnv') of
                                  (Just (Binding mut _), Just (Binding _ vcap)) -> M.insert k (Binding mut vcap) e
                                  _ -> e
                             ) env (M.keys newCaptured)
      pure (envUpdated, result)
apply env (VComposed funs) args = do
  let x = if null args then VNil else head args
  v' <- foldlM (\acc f -> snd <$> apply env f [acc]) x funs
  pure (env, v')
apply _ v _ = runtimeError ("Value is not callable: " ++ vType v)

applyBuiltin :: Env -> String -> [RVal] -> [RVal] -> IO (Env, RVal)
applyBuiltin env name caps args = case name of
  "puts" -> do
    putStrLn (unwords (map ((++"") . repr) args) ++ " ")
    pure (env, VNil)
  "+" -> builtinArity2 env name caps args (\l r -> evalInfix "+" l r)
  "-" -> builtinArity2 env name caps args (\l r -> evalInfix "-" l r)
  "*" -> builtinArity2 env name caps args (\l r -> evalInfix "*" l r)
  "/" -> builtinArity2 env name caps args (\l r -> evalInfix "/" l r)
  "push" -> case caps ++ args of
    [v, VList xs] -> pure (env, VList (xs ++ [v]))
    [v, VSet s]   -> pure (env, VSet (S.insert v s))
    [v]           -> pure (env, VPartial "push" [v])
    _             -> runtimeError ("Identifier can not be found: " ++ name)
  "assoc" -> case caps ++ args of
    [k, v, VDict m] -> case k of
      VDict _ -> runtimeError "Unable to use a Dictionary as a Dictionary key"
      _       -> pure (env, VDict (M.insert k v m))
    [k, v] -> pure (env, VPartial "assoc" [k, v])
    [k]    -> pure (env, VPartial "assoc" [k])
    _ -> runtimeError ("Identifier can not be found: " ++ name)
  "first" -> case caps ++ args of
    [VList []]    -> pure (env, VNil)
    [VList (x:_)] -> pure (env, x)
    [VStr ""]     -> pure (env, VNil)
    [VStr (c:_)] -> pure (env, VStr [c])
    []            -> pure (env, VPartial "first" [])
    _             -> runtimeError ("Identifier can not be found: " ++ name)
  "rest" -> case caps ++ args of
    [VList []]     -> pure (env, VList [])
    [VList (_:xs)] -> pure (env, VList xs)
    [VStr ""]      -> pure (env, VStr "")
    [VStr (_:cs)]  -> pure (env, VStr cs)
    []             -> pure (env, VPartial "rest" [])
    _              -> runtimeError ("Identifier can not be found: " ++ name)
  "size" -> case caps ++ args of
    [VList xs] -> pure (env, VInt (fromIntegral (length xs)))
    [VSet s]   -> pure (env, VInt (fromIntegral (S.size s)))
    [VDict m]  -> pure (env, VInt (fromIntegral (M.size m)))
    [VStr s]   -> pure (env, VInt (fromIntegral (lengthUtf8 s)))
    []         -> pure (env, VPartial "size" [])
    _          -> runtimeError ("Identifier can not be found: " ++ name)
  "map" -> case caps ++ args of
    [fn, VList xs] -> do
      if not (isFunctionVal fn)
        then runtimeError ("Unexpected argument: map(" ++ vType fn ++ ", List)")
        else do
          ys <- mapM (\x -> snd <$> call1 env fn x) xs
          pure (env, VList ys)
    [fn] -> pure (env, VPartial "map" [fn])
    [v1, v2] -> runtimeError ("Unexpected argument: map(" ++ vType v1 ++ ", " ++ vType v2 ++ ")")
    _ -> runtimeError ("Identifier can not be found: " ++ name)
  "filter" -> case caps ++ args of
    [fn, VList xs] -> do
      if not (isFunctionVal fn)
        then runtimeError ("Unexpected argument: filter(" ++ vType fn ++ ", List)")
        else do
          ys <- filterM (\x -> do (_, b) <- call1 env fn x; pure (truthy b)) xs
          pure (env, VList ys)
    [fn] -> pure (env, VPartial "filter" [fn])
    [v1, v2] -> runtimeError ("Unexpected argument: filter(" ++ vType v1 ++ ", " ++ vType v2 ++ ")")
    _ -> runtimeError ("Identifier can not be found: " ++ name)
  "fold" -> case caps ++ args of
    [initV, fn, VList xs] -> do
      if not (isFunctionVal fn)
        then runtimeError ("Unexpected argument: fold(" ++ vType initV ++ ", " ++ vType fn ++ ", List)")
        else do
          res <- foldlM (\acc x -> snd <$> call2 env fn acc x) initV xs
          pure (env, res)
    [initV, fn] -> pure (env, VPartial "fold" [initV, fn])
    [v1, v2, v3] -> runtimeError ("Unexpected argument: fold(" ++ vType v1 ++ ", " ++ vType v2 ++ ", " ++ vType v3 ++ ")")
    _ -> runtimeError ("Identifier can not be found: " ++ name)
  _    -> runtimeError ("Identifier can not be found: " ++ name)

builtinArity2 :: Env -> String -> [RVal] -> [RVal] -> (RVal -> RVal -> IO RVal) -> IO (Env, RVal)
builtinArity2 env name caps args f = case caps ++ args of
  [a,b] -> do r <- f a b; pure (env, r)
  [a]   -> pure (env, VPartial name [a])
  _     -> runtimeError ("Identifier can not be found: " ++ name)

bin2 :: Env -> String -> [RVal] -> (RVal -> RVal -> IO RVal) -> IO (Env, RVal)
bin2 env name args f = case args of
  [l, r] -> do v <- f l r; pure (env, v)
  _      -> runtimeError ("Value is not callable: " ++ name)

evalInfix :: String -> RVal -> RVal -> IO RVal
evalInfix op l r = case op of
  "+" -> case (l, r) of
    (VInt a, VInt b)   -> pure (VInt (a + b))
    (VDec a, VDec b)   -> pure (normalize (VDec (a + b)))
    (VInt a, VDec b)   -> pure (normalize (VDec (fromInteger a + b)))
    (VDec a, VInt b)   -> pure (normalize (VDec (a + fromInteger b)))
    (VStr a, VStr b)   -> pure (VStr (a ++ b))
    (VStr a, b)        -> pure (VStr (a ++ repr b))
    (a, VStr b)        -> pure (VStr (repr a ++ b))
    (VList a, VList b) -> pure (VList (a ++ b))
    (VSet a, VSet b)   -> pure (VSet (S.union a b))
    (VDict a, VDict b) -> pure (VDict (M.unionWith (\_ right -> right) a b))
    _ -> unsupported op l r
  "-" -> case (l, r) of
    (VInt a, VInt b) -> pure (VInt (a - b))
    (VDec a, VDec b) -> pure (normalize (VDec (a - b)))
    (VInt a, VDec b) -> pure (normalize (VDec (fromInteger a - b)))
    (VDec a, VInt b) -> pure (normalize (VDec (a - fromInteger b)))
    _ -> unsupported op l r
  "*" -> case (l, r) of
    (VInt a, VInt b) -> pure (VInt (a * b))
    (VDec a, VDec b) -> pure (normalize (VDec (a * b)))
    (VInt a, VDec b) -> pure (normalize (VDec (fromInteger a * b)))
    (VDec a, VInt b) -> pure (normalize (VDec (a * fromInteger b)))
    (VStr s, VInt n)
      | n == 0        -> pure (VStr "")
      | n > 0         -> pure (VStr (concat (replicate (fromInteger n) s)))
      | otherwise     -> runtimeError ("Unsupported operation: String * Integer (< 0)")
    (VStr _, VDec _) -> runtimeError ("Unsupported operation: String * Decimal")
    _ -> unsupported op l r
  "/" -> case (l, r) of
    (_, VInt 0) -> runtimeError "Division by zero"
    (_, VDec 0.0) -> runtimeError "Division by zero"
    (VInt a, VInt b) -> pure (VInt (a `quot` b))
    (VDec a, VDec b) -> pure (normalize (VDec (a / b)))
    (VInt a, VDec b) -> pure (normalize (VDec (fromInteger a / b)))
    (VDec a, VInt b) -> pure (normalize (VDec (a / fromInteger b)))
    _ -> unsupported op l r
  "&&" -> pure (VBool (truthy l && truthy r))
  "||" -> pure (VBool (truthy l || truthy r))
  "==" -> pure (VBool (eqVal l r))
  "!=" -> pure (VBool (not (eqVal l r)))
  ">" -> compOp (>) l r
  "<" -> compOp (<) l r
  ">=" -> compOp (>=) l r
  "<=" -> compOp (<=) l r
  _ -> unsupported op l r

  where
    compOp :: (Double -> Double -> Bool) -> RVal -> RVal -> IO RVal
    compOp f a b = case (a, b) of
      (VInt x, VInt y) -> pure (VBool (f (fromInteger x) (fromInteger y)))
      (VDec x, VDec y) -> pure (VBool (f x y))
      (VInt x, VDec y) -> pure (VBool (f (fromInteger x) y))
      (VDec x, VInt y) -> pure (VBool (f x (fromInteger y)))
      (VStr x, VStr y) -> pure (VBool (f' x y))
      _ -> unsupported op a b
      where
        f' :: String -> String -> Bool
        f' = case show op of
               ">"  -> (>)
               "<"  -> (<)
               ">=" -> (>=)
               "<=" -> (<=)
               _     -> (==)

truthy :: RVal -> Bool
truthy v = case v of
  VNil     -> False
  VBool b  -> b
  VInt i   -> i /= 0
  VDec d   -> d /= 0.0
  VStr s   -> not (null s)
  VList xs -> not (null xs)
  VSet s   -> not (S.null s)
  VDict m  -> not (M.null m)
  VBuiltin _ -> True
  VClosure{} -> True
  VPartial{} -> True
  VComposed{} -> True

unsupported :: String -> RVal -> RVal -> IO RVal
unsupported op l r = runtimeError ("Unsupported operation: " ++ vType l ++ " " ++ op ++ " " ++ vType r)

vType :: RVal -> String
vType v = case v of
  VInt _   -> "Integer"
  VDec _   -> "Decimal"
  VStr _   -> "String"
  VBool _  -> "Boolean"
  VNil     -> "Nil"
  VList _  -> "List"
  VSet _   -> "Set"
  VDict _  -> "Dictionary"
  VBuiltin _ -> "Function"
  VClosure{} -> "Function"
  VPartial{} -> "Function"
  VComposed{} -> "Function"

repr :: RVal -> String
repr v = case v of
  VInt i  -> show i
  VDec d  -> showFFloat Nothing d ""
  VBool b -> if b then "true" else "false"
  VNil    -> "nil"
  VStr s  -> if s == "\"" then "\"\"\"" else ('"' : concatMap escRaw s ++ "\"")
  VList xs -> "[" ++ joinBy ", " (map repr xs) ++ "]"
  VSet s   -> "{" ++ joinBy ", " (map repr (S.toAscList s)) ++ "}"
  VDict m  ->
    let pairs = M.toAscList m
        showPair (k,v') = repr k ++ ": " ++ repr v'
    in "#{" ++ joinBy ", " (map showPair pairs) ++ "}"
  VClosure{} -> "<fn>"
  VComposed{} -> "<fn>"
  VPartial{} -> "<fn>"
  VBuiltin _ -> "<fn>"
  where
    escRaw '"'  = "\\\""
    escRaw '\\' = "\\\\"
    escRaw '\n' = "\\n"
    escRaw '\t' = "\\t"
    escRaw c     = [c]


runtimeError :: String -> IO a
runtimeError msg = do
  putStrLn ("[Error] " ++ msg)
  exitFailure

typeErrorUnary :: String -> RVal -> IO a
typeErrorUnary op v = runtimeError ("Unsupported operation: " ++ op ++ " " ++ vType v)

parseInt :: String -> Integer
parseInt s = read (filter (/= '_') s)

parseDec :: String -> Double
parseDec s = read (filter (/= '_') s)

normalize :: RVal -> RVal
normalize (VDec d) =
  let r = fromInteger (round d) :: Double
  in if abs (d - r) < 1e-12 then VInt (round d) else VDec d
normalize v = v

-- Helpers for collections, indexing, equality, and ordering

joinBy :: String -> [String] -> String
joinBy _ [] = ""
joinBy _ [x] = x
joinBy sep (x:xs) = x ++ sep ++ joinBy sep xs

lengthUtf8 :: String -> Int
lengthUtf8 = BS.length . TE.encodeUtf8 . T.pack

-- Indexing with error handling
evalIndex :: RVal -> RVal -> IO RVal
evalIndex base idx = case base of
  VList xs -> case idx of
    VInt i -> pure (indexList xs i)
    VDec _ -> indexError "List" "Decimal"
    VBool _ -> indexError "List" "Boolean"
    VStr _ -> indexError "List" "String"
    VNil -> indexError "List" "Nil"
    VList _ -> indexError "List" "List"
    VSet _ -> indexError "List" "Set"
    VDict _ -> indexError "List" "Dictionary"
    VBuiltin _ -> indexError "List" "Function"
  VStr s -> case idx of
    VInt i -> pure (indexString s i)
    VDec _ -> indexError "String" "Decimal"
    VBool _ -> indexError "String" "Boolean"
    VStr _ -> indexError "String" "String"
    VNil -> indexError "String" "Nil"
    VList _ -> indexError "String" "List"
    VSet _ -> indexError "String" "Set"
    VDict _ -> indexError "String" "Dictionary"
    VBuiltin _ -> indexError "String" "Function"
  VDict m -> case M.lookup idx m of
    Just v  -> pure v
    Nothing -> pure VNil
  _ -> pure VNil

indexError :: String -> String -> IO a
indexError contTy idxTy = runtimeError ("Unable to perform index operation, found: " ++ contTy ++ "[" ++ idxTy ++ "]")

indexList :: [RVal] -> Integer -> RVal
indexList xs i
  | null xs = VNil
  | i >= 0 && fromIntegral i < length xs = xs !! fromIntegral i
  | i < 0 && abs i <= fromIntegral (length xs) = xs !! (length xs + fromIntegral i)
  | otherwise = VNil

indexString :: String -> Integer -> RVal
indexString s i
  | null s = VNil
  | i >= 0 && fromIntegral i < length s = VStr [s !! fromIntegral i]
  | i < 0 && abs i <= fromIntegral (length s) = VStr [s !! (length s + fromIntegral i)]
  | otherwise = VNil

-- Structural equality
eqVal :: RVal -> RVal -> Bool
eqVal (VInt a) (VInt b) = a == b
eqVal (VDec a) (VDec b) = a == b
eqVal (VStr a) (VStr b) = a == b
eqVal (VBool a) (VBool b) = a == b
eqVal VNil VNil = True
eqVal (VList a) (VList b) = length a == length b && and (zipWith eqVal a b)
eqVal (VSet a) (VSet b) = S.size a == S.size b && S.isSubsetOf a b
eqVal (VDict a) (VDict b) = M.size a == M.size b && M.foldlWithKey' (\acc k v -> acc && case M.lookup k b of
                                                                  Just v' -> eqVal v v'
                                                                  Nothing -> False) True a
eqVal _ _ = False

-- Provide Ord for RVal to allow use in Set/Map; deterministic ordering
instance Eq RVal where
  (==) = eqVal

instance Ord RVal where
  compare a b = case (a, b) of
    (VNil, VNil) -> EQ
    (VNil, _) -> LT
    (_, VNil) -> GT
    (VBool x, VBool y) -> compare x y
    (VBool _, _) -> LT
    (_, VBool _) -> GT
    (VInt x, VInt y) -> compare x y
    (VInt _, _) -> LT
    (_, VInt _) -> GT
    (VDec x, VDec y) -> compare x y
    (VDec _, _) -> LT
    (_, VDec _) -> GT
    (VStr x, VStr y) -> compare x y
    (VStr _, _) -> LT
    (_, VStr _) -> GT
    (VList xs, VList ys) -> compare xs ys
    (VList _, _) -> LT
    (_, VList _) -> GT
    (VSet xs, VSet ys) -> compare (S.toAscList xs) (S.toAscList ys)
    (VSet _, _) -> LT
    (_, VSet _) -> GT
    (VDict xm, VDict ym) -> compare (M.toAscList xm) (M.toAscList ym)
    (VDict _, _) -> LT
    (_, VDict _) -> GT
    (VBuiltin x, VBuiltin y) -> compare x y
    (VBuiltin _, _) -> LT
    (_, VBuiltin _) -> GT
    (VClosure _ ps1 b1, VClosure _ ps2 b2) -> compare (length ps1) (length ps2)
    (VClosure{}, _) -> LT
    (_, VClosure{}) -> GT
    (VComposed a1, VComposed a2) -> compare (length a1) (length a2)
    (VComposed{}, _) -> LT
    (_, VComposed{}) -> GT
    (VPartial n1 c1, VPartial n2 c2) -> compare (n1, length c1) (n2, length c2)
    (VPartial{}, _) -> LT
    (_, VPartial{}) -> GT

isDictLiteral :: Expr -> Bool
isDictLiteral (EDict _) = True
isDictLiteral _ = False

-- Helpers for function calls
filterM :: Monad m => (a -> m Bool) -> [a] -> m [a]
filterM _ [] = return []
filterM p (x:xs) = do
  flg <- p x
  ys <- filterM p xs
  return (if flg then x:ys else ys)

foldlM :: Monad m => (b -> a -> m b) -> b -> [a] -> m b
foldlM _ z [] = return z
foldlM f z (x:xs) = do
  z' <- f z x
  foldlM f z' xs

call1 :: Env -> RVal -> RVal -> IO (Env, RVal)
call1 env fn a = apply env fn [a]

call2 :: Env -> RVal -> RVal -> RVal -> IO (Env, RVal)
call2 env fn a b = apply env fn [a,b]

wrapVal :: RVal -> Expr
wrapVal v = case v of
  VInt i -> EInteger (show i)
  VDec d -> EDecimal (showFFloat Nothing d "")
  VStr s -> EString s
  VBool b -> if b then EBoolean True else EBoolean False
  VNil -> ENil
  VList xs -> EList (map wrapVal xs)
  VSet s -> ESet (map wrapVal (S.toList s))
  VDict m -> EDict (map (\(k,v') -> (wrapVal k, wrapVal v')) (M.toList m))
  VBuiltin n -> EIdentifier n
  VClosure{} -> EIdentifier "<fn>"
  VComposed{} -> EIdentifier "<fn>"

threadApply :: Env -> RVal -> [Expr] -> IO RVal
threadApply env v [] = pure v
threadApply env v (e:es) = case e of
  ECall f args -> do
    (env1, fv) <- evalExpr env f
    (env2, argVals) <- evalArgs env1 args
    (_, v') <- apply env2 fv (argVals ++ [v])
    threadApply env2 v' es
  _ -> do
    (env1, fv) <- evalExpr env e
    (_, v') <- apply env1 fv [v]
    threadApply env1 v' es

evalArgs :: Env -> [Expr] -> IO (Env, [RVal])
evalArgs env [] = pure (env, [])
evalArgs env (a:as) = do
  (env1, v) <- evalExpr env a
  (env2, vs) <- evalArgs env1 as
  pure (env2, v:vs)

isFunctionVal :: RVal -> Bool
isFunctionVal v = case v of
  VBuiltin _  -> True
  VClosure{}  -> True
  VPartial{}  -> True
  VComposed{} -> True
  _           -> False

builtin2 :: Env -> String -> [RVal] -> (RVal -> RVal -> IO RVal) -> IO (Env, RVal)
builtin2 env _ [a,b] f = do r <- f a b; pure (env, r)
builtin2 env name [a] f = do
  ref <- newIORef env
  let body = Block [SExpression (ECall (EIdentifier name) [EIdentifier "_x", EIdentifier "_y"])]
  pure (env, VClosure ref ["_x","_y"] body) -- generic 2-arg closure; args will be ignored if body doesn't use
builtin2 _ name args _ = runtimeError ("Identifier can not be found: " ++ name)
