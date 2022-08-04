# Getting Started

Add the GitLab repo to your conan remotes:

```
conan remote add gitlab https://gitlab.com/api/v4/packages/conan
```

Now a package ref to your Conan project requirements (conanfile.py):

```
    requires = [
        ...
        "batteries/0.8.0@tonyastolfi+batteries/stable",
    ]

```

Check out the [documentation](https://batteriescpp.github.io/)!
