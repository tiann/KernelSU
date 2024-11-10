# 非公式の対応デバイス

::: warning 警告
このページでは他の開発者が管理している、KernelSU をサポートする GKI 以外のデバイス用のカーネルを紹介しています。
:::

::: warning 警告
このページはあなたのデバイスに対応するソースコードを見つけるためのものであり、そのソースコードが _KernelSU 開発者_ によってレビューされたことを意味するものではありません。ご自身の責任においてご利用ください。
:::

<script setup>
import data from '../../repos.json'
</script>

<table>
   <thead>
      <tr>
         <th>メンテナー</th>
         <th>リポジトリ</th>
         <th>対応デバイス</th>
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