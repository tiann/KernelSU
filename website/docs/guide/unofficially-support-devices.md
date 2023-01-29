# Unofficially Supported Devices

::: warning
In this page, there are kernels for non-GKI devices supporting KernelSU maintained by other developers.
:::

::: warning
This page is only for you to find the source code corresponding to your device, it does **NOT** mean that the source code has been reviewed by _KernelSU Developers_. You should use it at your own risk.
:::

<script setup>
import data from '../repos'
</script>

<table>
   <thead>
      <tr>
         <th>Maintainer</th>
         <th>Repository</th>
         <th>Support devices</th>
      </tr>
   </thead>
   <tbody>
    <tr v-for="repo in data" :key="repo.devices">
        <td><a :href="repo.maintainer_link" target="_blank" rel="noreferrer">SakuraNotStupid</a></td>
        <td><a :href="repo.kernel_link" target="_blank" rel="noreferrer">android_kernel_xiaomi_sdm710</a></td>
        <td>MI 8 SE: sirius</td>
    </tr>
   </tbody>
</table>
