# Dispositivos não oficialmente suportados

::: aviso
Nesta página, existem kernels para dispositivos não GKI que suportam KernelSU mantidos por outros desenvolvedores.
:::

::: aviso
Esta página é apenas para você encontrar o código-fonte correspondente ao seu dispositivo. **NÃO** significa que o código-fonte foi revisado por _KernelSU Developers_. Você deve usá-lo por sua própria conta e risco.
:::

<script setup>
import data from '../repos.json'
</script>

<table>
   <thead>
      <tr>
         <th>Mantenedor</th>
         <th>Repositório</th>
         <th>Dispositivos suportados</th>
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