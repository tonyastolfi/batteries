{
  "version": "0.2.0",
  "configurations": map(. | 
    {
      "type": "vgdb",
      "request": "launch",
      "name": (.build_type + "/" + .name + " (C/C++ Debug)"),
      "program": .path,
      "args": [],
      "env": .env,
      "cwd": $ENV["project_dir"]
    }
  )
}
