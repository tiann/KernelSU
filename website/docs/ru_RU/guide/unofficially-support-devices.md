# Неофициально поддерживаемые устройства

::: warning
На этой странице представлены ядра для устройств, не поддерживающих ГКИ и поддерживающих KernelSU, которые поддерживаются другими разработчиками.
:::

::: warning
Эта страница предназначена только для поиска исходного кода, соответствующего вашему устройству, и **НЕ** означает, что исходный код был проверен _разработчиками KernelSU_. Вы должны использовать его на свой страх и риск.
:::

<script setup>
import data from '../../repos.json'
</script>

<table>
   <thead>
      <tr>
         <th>Сопровождающий</th>
         <th>Репозиторий</th>
         <th>Поддерживаемое устройство</th>
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
