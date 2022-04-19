# How to Configure CI Pipelines for Batteries in GitLab

## Create a Deploy Token for the Repository

This step is necessary to allow Conan to access the Package Registry for your project so releases can be published.

Navigate to your repo on GitLab.  Go to  Settings &gt; Repository &gt; Deploy Tokens

Enter the following:

- Name: ci-deployment-token
- Expiration date (optional): <whatever-date-you-want>
- Username (optional): <leave-blank>

For the "Scopes" section, click the checkbox next to `write_package_registry` (this is the only required capability).

NOTE: The name "ci-deployment-token" MUST be entered exactly as shown; this is a special magic token name that GitLab uses to infer that the token should be injected via environment variables to CI/CD pipelines.

Click "Create deploy token."  You can save the token value if you want, but you don't have to; it will be automatically injected into your pipeline jobs from now on.

## Set CI/CD Variables 

This step is necessary to tell Conan where to push released packages, and under what username and channel.

_NOTE: The instructions for this step assume that any Conan remote you are going to use have been configured in `docker/Dockerfile`.  For example:_

```docker
...

# Point at various release package repos.
#
RUN conan remote add gitlab https://gitlab.com/api/v4/packages/conan

...
```

Navigate to your repo on GitLab.  Go to  Settings &gt; CI/CD &gt; Variables &gt; Expand

You must create three variables:

- `RELEASE_CONAN_USER`: The user under which to release packages; this should be the name of your repo on GitLab, with '+' substituted for '/'.  For example, if your project lives at `https://gitlab.com/janedoe/batteries_fork`, the Conan user should be `janedoe+batteries_fork`
- `RELEASE_CONAN_CHANNEL`: The channel under which to release packages (for example, "stable", "unstable", or "testing")
- `RELEASE_CONAN_REMOTE`: The Conan remote name for the package registry to which releases should be published

When you publish a release (e.g., 1.7.3), the Conan "recipe name" will be `batteries/1.7.3@$RELEASE_CONAN_USER/$RELEASE_CONAN_CHANNEL`.

For each variable, leave the Type as "Variable" and the Environment scope as "All (default)".  Under "Flags", "Protect variable" and "Mask variable" should both be _unchecked_.

## Build the Pipeline Docker Image and Upload to GitLab

1. Clone a local repo:
   ```shell
   git clone https://gitlab.com/tonyastolfi/batteries
   ```
2. Enter the local repo directory:
   ```shell
   cd batteries/
   ```
3. Build the docker image:
   ```shell
   make docker-build
   ```
4. Create an Access Token to login to the GitLab Docker Container Registry:
   1. Go to (Your Profile Picture in the upper-right corner) &gt; Edit profile (in the drop-down) &gt; Access Tokens (in the left-sidebar)
   2. Enter a token name and expiration date (can be anything)
   3. Select scope "api"
   4. Click "Create personal access token"
   5. **IMPORTANT: Write down the access token value (or copy-paste it somewhere)**  Otherwise you will need to do these steps over again!
5. (first time only) Log in to the GitLab Container Registry using the access token you just created (you can retrieve the correct command for your GitLab instance by going to (your repo) &gt; Packages &amp; Registries &gt; Container Registry and clicking the CLI Commands drop down button in the upper-right):
   ```shell
   docker login registry.gitlab.com
   ```
6. Upload the container image you built in step 3:
   ```shell
   make docker-push
   ```

If you want to check to see if this succeeded, you can go to (your repo) &gt; Packages &amp; Registries &gt; Container Registry to see a list of the containers that have been added to your registry.

## Configure GitLab Runners

_NOTE: This step is optional if your GitLab instance already has CI Runners configured in the scope of your project.  You can check whether this is the case by navigating to (your repo) &gt; Settings &gt; CI/CD &gt; Runners &gt; Expand._

### Install GitLab Runner on your hardware

#### GNU/Linux (x86_64)

##### Download

Follow the instructions at [Linux Manual Install](https://docs.gitlab.com/runner/install/linux-manually.html#using-binary-file) option (Linux x86-64) to download the gitlab-runner binary:

```shell
sudo curl -L --output /usr/local/bin/gitlab-runner "https://gitlab-runner-downloads.s3.amazonaws.com/latest/binaries/gitlab-runner-linux-amd64"
```

##### Create gitlab-runner user

You can put the home directory for this user wherever you want; it was convenient for my setup to place it under `/local/home`.

```shell
mkdir -p /local/home/ && sudo useradd gitlab-runner --comment 'GitLab Runner' --home /local/home/gitlab-runner --create-home --shell /bin/bash
```

##### "Install" the runner

```shell
sudo gitlab-runner install --user=gitlab-runner --working-directory=/local/home/gitlab-runner
```

##### Start the runner

```
sudo gitlab-runner start
```

##### Register the runner with GitLab

Go to (your repo) &gt; Settings &gt; CI/CD &gt; Runners &gt; Expand.

Under the column "Specific runners," copy the registration token string.

On the machine where you installed gitlab-runner:

```shell
sudo gitlab-runner register
```

This will prompt you for various pieces of information.  Paste the registration token string you copied above when prompted.

If this step is successful, you will see your runner show up on the Settings &gt; CI/CD &gt; Runners page in GitLab.
