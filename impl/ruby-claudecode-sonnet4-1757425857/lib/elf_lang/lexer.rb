module ElfLang
  class Lexer
    KEYWORDS = {
      'let' => 'LET',
      'mut' => 'MUT', 
      'if' => 'IF',
      'else' => 'ELSE',
      'true' => 'TRUE',
      'false' => 'FALSE',
      'nil' => 'NIL'
    }
    
    def initialize(source)
      @source = source
      @pos = 0
      @tokens = []
    end
    
    def tokenize
      while @pos < @source.length
        skip_whitespace
        break if @pos >= @source.length
        
        char = current_char
        
        case char
        when '/'
          if peek == '/'
            tokenize_comment
          else
            add_token('/', '/')
            advance
          end
        when '+'
          add_token('+', '+')
          advance
        when '-'
          add_token('-', '-')
          advance
        when '*'
          add_token('*', '*')
          advance
        when '='
          if peek == '='
            add_token('==', '==')
            advance(2)
          else
            add_token('=', '=')
            advance
          end
        when '!'
          if peek == '='
            add_token('!=', '!=')
            advance(2)
          else
            raise "Unexpected character: #{char}"
          end
        when '>'
          if peek == '='
            add_token('>=', '>=')
            advance(2)
          elsif peek == '>'
            add_token('>>', '>>')
            advance(2)
          else
            add_token('>', '>')
            advance
          end
        when '<'
          if peek == '='
            add_token('<=', '<=')
            advance(2)
          else
            add_token('<', '<')
            advance
          end
        when '&'
          if peek == '&'
            add_token('&&', '&&')
            advance(2)
          else
            raise "Unexpected character: #{char}"
          end
        when '|'
          if peek == '|'
            add_token('||', '||')
            advance(2)
          elsif peek == '>'
            add_token('|>', '|>')
            advance(2)
          else
            add_token('|', '|')
            advance
          end
        when '{'
          add_token('{', '{')
          advance
        when '}'
          add_token('}', '}')
          advance
        when '['
          add_token('[', '[')
          advance
        when ']'
          add_token(']', ']')
          advance
        when '#'
          if peek == '{'
            add_token('#{', '#{')
            advance(2)
          else
            raise "Unexpected character: #{char}"
          end
        when ';'
          add_token(';', ';')
          advance
        when '('
          add_token('(', '(')
          advance
        when ')'
          add_token(')', ')')
          advance
        when ','
          add_token(',', ',')
          advance
        when ':'
          add_token(':', ':')
          advance
        when '"'
          tokenize_string
        when /[0-9]/
          tokenize_number
        when /[a-zA-Z_]/
          tokenize_identifier_or_keyword
        else
          raise "Unexpected character: #{char}"
        end
      end
      
      @tokens
    end
    
    private
    
    def current_char
      @source[@pos]
    end
    
    def peek(offset = 1)
      pos = @pos + offset
      pos < @source.length ? @source[pos] : nil
    end
    
    def advance(count = 1)
      @pos += count
    end
    
    def skip_whitespace
      while @pos < @source.length && current_char =~ /\s/
        advance
      end
    end
    
    def add_token(type, value)
      @tokens << { 'type' => type, 'value' => value }
    end
    
    def tokenize_comment
      start_pos = @pos
      while @pos < @source.length && current_char != "\n"
        advance
      end
      
      comment_text = @source[start_pos...@pos]
      add_token('CMT', comment_text)
      
      # Skip the newline character if present
      if @pos < @source.length && current_char == "\n"
        advance
      end
    end
    
    def tokenize_string
      start_pos = @pos
      advance # skip opening quote
      
      result = '"'
      
      while @pos < @source.length && current_char != '"'
        if current_char == '\\'
          advance
          if @pos >= @source.length
            raise "Unterminated string"
          end
          
          escape_char = current_char
          case escape_char
          when 'n'
            result += '\\n'
          when 't'
            result += '\\t'
          when '"'
            result += '\\"'
          when '\\'
            result += '\\\\'
          else
            result += '\\' + escape_char
          end
          advance
        else
          result += current_char
          advance
        end
      end
      
      if @pos >= @source.length
        raise "Unterminated string"
      end
      
      result += '"'
      advance # skip closing quote
      
      add_token('STR', result)
    end
    
    def tokenize_number
      start_pos = @pos
      has_decimal_point = false
      
      while @pos < @source.length
        char = current_char
        
        if char =~ /[0-9]/
          advance
        elsif char == '_' && @pos > start_pos && @pos + 1 < @source.length && @source[@pos + 1] =~ /[0-9]/
          advance
        elsif char == '.' && !has_decimal_point && @pos + 1 < @source.length && @source[@pos + 1] =~ /[0-9]/
          has_decimal_point = true
          advance
        else
          break
        end
      end
      
      number_text = @source[start_pos...@pos]
      
      if has_decimal_point
        add_token('DEC', number_text)
      else
        add_token('INT', number_text)
      end
    end
    
    def tokenize_identifier_or_keyword
      start_pos = @pos
      
      while @pos < @source.length && (current_char =~ /[a-zA-Z0-9_]/)
        advance
      end
      
      text = @source[start_pos...@pos]
      token_type = KEYWORDS[text] || 'ID'
      
      add_token(token_type, text)
    end
  end
end