# Thiết bị hỗ trợ không chính thức

::: warning
Đây là trang liệt kê kernel cho các thiết bị không dùng GKI được hỗ trợ bởi các lập trình viên khác. 

:::

::: warning
Trang này chỉ để cho bạn tìm thấy source cho thiết bị của bạn, nó **HOÀN TOÀN KHÔNG** được review bởi _lập trình viên của KernelSU_. Vậy nên hãy tự chấp nhận rủi ro khi sử dụng chúng.

:::

<script setup>
import data from '../../repos.json'
</script>

<table>
   <thead>
      <tr>
         <th>Maintainer</th>
         <th>Repository</th>
         <th>Thiết bị hỗ trợ</th>
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
