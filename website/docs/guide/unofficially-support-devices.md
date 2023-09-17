# Unofficially Supported Devices

::: warning
In this page, there are kernels for non-GKI devices supporting KernelSU maintained by other developers.
:::

::: warning
This page is only for you to find the source code corresponding to your device, it does **NOT** mean that the source code has been reviewed by _KernelSU Developers_. You should use it at your own risk.
:::

<script setup>
import data from '../repos.json'
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
        <td><a :href="https://github.com/zharzinhoo" target="_blank" rel="noreferrer">zharzinhoo</a></td>
        <td><a :href="https://github.com/zharzinhoo/Kernel-Oriente-Guamp" target="_blank" rel="noreferrer">Oriente Kernel Guamp</a></td>
        <td>guamp</td>
    </tr>
   </tbody>
</table>
