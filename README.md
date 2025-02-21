# Description

InfinyonAnalytics is an Unreal Engine plugin designed to integrate analytics
into your Unreal Engine 5.5 projects. This plugin allows developers to collect,
data and send it to a Fluvio cluster running on the Infinyon Cloud for analysis
and visualization.

## Adding InfinyonAnalytics to Your Unreal Engine 5.5 Project

1. **Get the Plugin**:
    - The plugin can be check out directly from this repository, or download
      a version from github

2. **Install the Plugin**:
    - In the project 'Plugins' directory, checkout the plugin or extract the
      downloaded plugin files into a folder named `InfinyonAnalytics`
    - If the `Plugins` directory does not exist, create it in the root of your
      project directory.

3. **Enable the Plugin**:
    - Open your Unreal Engine 5.5 project.
    - Navigate to `Edit` > `Plugins`.
    - In the Plugins window, search for `InfinyonAnalytics`.
    - Check the box to enable the plugin.
    - Also enable
    - Restart the Unreal Engine editor to apply the changes.

4. **Configure the Plugin**:
    - Go to Infinyon.com and setup a fluvio cluster and create an access-key for
      an analytics storage topic.
      https://www.fluvio.io/docs/latest/cloud/how-to/use-ws-gateway

5. **Using the Plugin**:
    - The standard Unreal Engine Analytics Event interfaces can be used to
      create Analytics Events with attributed from the blueprint editor or
      from code.

Visit us on Discord if you have any questions or comments
https://discord.com/invite/bBG2dTz

