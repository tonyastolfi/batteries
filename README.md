# Getting Started

Add the batteriesincuded repo to your conan remotes:

```
conan remote add batteriesincluded https://api.bintray.com/conan/batteriesincluded/conan 
```

Add a ref to batteries (latest release=0.2.0):

```
conan remote add_ref batteries/0.2.0@demo/testing batteriesincluded
```

Now add this to your conan project requirements (conanfile.py):

```
    requires = [
        ...
        "batteries/0.2.0@demo/testing",
    ]

```
