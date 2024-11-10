# 非官方支援裝置

::: warning 警告
本文件列出由其他開發人員維護的支援 KernelSU 的非 GKI 裝置核心
:::

::: warning 警告
本文件僅便於尋找裝置對應原始碼，這並非意味著這些原始碼被 KernelSU 開發人員**審查**，您應自行承擔風險。
:::

<script setup>
import data from '../../repos.json'
</script>

<table>
   <thead>
      <tr>
         <th>維護者</th>
         <th>存放庫</th>
         <th>支援裝置</th>
      </tr>
   </thead>
   <tbody>
    <tr v-for="repo in data" :key="repo.devices">
        <td><a :href="repo.maintainer_link" target="_blank" rel="noreferrer">{{ repo.maintainer }}</a></td>
        <td><a :href="repo.kernel_link" target="_blank" rel="noreferrer">{{ repo.kernel_name }}</a></td>
        <td>{{ repo.devices }}</td>
    </tr>
   </tbody>
</table>