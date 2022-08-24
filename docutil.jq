#######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
# Copyright 2022 Anthony Paul Astolfi


# True iff the input is a string containing an ascii art line.
#
def is_ascii_art_line:
    . | test( "^[=#+\\- ]+$" );

# Transform an object with .Name and .Namespace into a C++ fully
# qualified name string.
#
def make_qualified_name:
  . as $obj | (.Namespace | map(.Name) | reverse | join("::")) + "::" + ($obj | .Name);

def repair_type_name:
  . | gsub("_Bool"; "bool")? // "";

def make_param_str:
  . as $param | ($param | .Type | .Name) + " " + ($param | .Name);

def make_param_list_str:
  (. as $obj | "(" + ($obj | .Params? | arrays | map(make_param_str) | join(", ")) + ")")
  // "()";


def make_file_url:
  . as $obj |
  "https://gitlab.com/tonyastolfi/batteries/-/blob/main/src/"
    + ($obj | .Filename) + "#L" + ($obj | .LineNumber | tostring);

def add_file_urls:
  (.. | objects | select(has("Filename") and has("LineNumber")))
    |= (. as $obj | $obj + { "Url": $obj | make_file_url }); 

# Replace all .Kind == TextComment objects with the .Text string,
# replacing ascii art lines with "".
#
def process_text_comments:
  (.. | objects | select(.Kind == "TextComment"))
    |= ((.Text? // "") as $text | if ($text | is_ascii_art_line) then "" else $text end);

# Replace all .Kind == ParagraphComment objects with the string
# concentation of all its children, with a newline added (this is a
# paragraph after all!).
#
def process_paragraph_comments:
  (.. | objects | select(.Kind == "ParagraphComment"))
    |= (.Children as $children | $children | arrays | map(select(strings)) | join("") + "\n");

# Replace all .Kind == FullComment objects with the "\n"-joined string
# of all children.
#
def process_full_comments:
  (.. | objects | select(.Kind == "FullComment"))
    |= (.Children as $children | $children | arrays | map(select(strings)) | join("\n"));

# Replace all .Description values with template-friendly objects
# containing strings for Full description, Summary description, and
# Detail description (Full - Summary).
#
def process_descriptions:
  (.. | objects | select(has("Description")) | .Description) |= (. | map(select(strings)) | join("\n") | {
     "Full": . | ltrimstr(" ") | rtrimstr("\n"),
     "Summary": . | split(".") | (.[0] // "") | ltrimstr(" ") | rtrimstr("\n"),
     "Detail": . | split(".") | .[1:] | join(".") | ltrimstr(" ") | rtrimstr("\n")
  });

# Augment all objects having a Name and Namespace with their QualifiedName.
#
def add_qualified_names:
  (.. | objects | select(has("Namespace") and has("Name")))
    |= . as $obj | $obj + { "QualifiedName": $obj | make_qualified_name };

# Remove any full-path prefix from source file paths, turning them
# into paths relative to the "src" directory.
#
def shorten_filenames:
  (.. | objects | .Filename
    | strings) |= sub(".*/src/(./)*";"");


def make_display_name:
  . as $obj
  | if $obj | has("ReturnType") then
      (($obj | .QualifiedName) + ($obj | make_param_list_str))
    elif $obj | .TagType? == "Class" then
      ("class " + ($obj | .QualifiedName))
    else
      ($obj | .QualifiedName)
    end;

# Add "DisplayName" property to all symbol objects; DisplayName is
# QualifiedName plus the parameters (to distinguish between different
# function overloads).
#
def add_display_names:
  (.. | objects | select(has("QualifiedName")))
    |= (. as $obj | $obj + { "DisplayName": ($obj | make_display_name) });

def repair_type_names:
  (.. | objects | select(has("Type")) | .Type? | objects)
    |= . + { "Name": .Name | repair_type_name };

def add_location:
  (.. | objects | select(has("DefLocation") and (has("Location") | not)))
    |= . + { "Location": [.DefLocation] };

def add_header_files:
  (.. | objects | select(has("Location")))
    |= (. as $obj | $obj + { "HeaderFile": (($obj | .Location | .[0] | .Filename | sub("(_decl|_impl).hpp$"; ".hpp"))? // "")});

def all_symbols:
  [recurse];

def build_index($key):
  . as $doc
  | $doc | [..] | map(objects | .[$key] | strings as $value
  | $value | {
    key: .,
    value: ($doc | [..] | map(objects | select(.[$key] == $value)))
  }) | from_entries;


def process_clangdoc:
  process_text_comments
  | process_paragraph_comments 
  | process_full_comments 
  | process_descriptions
  | add_qualified_names
  | shorten_filenames
  | add_file_urls
  | repair_type_names
  | add_display_names
  | add_location
  | add_header_files;

      #| {
      #  "QualifiedName" : build_index("QualifiedName"),
      #  "Filename" : build_index("Filename"),                                     
      #  "USR" : build_index("USR"),                                     
      #};
