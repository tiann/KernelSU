package me.weishu.kernelsu.data.repository

import me.weishu.kernelsu.data.model.RepoModule

interface ModuleRepoRepository {
    suspend fun fetchModules(): Result<List<RepoModule>>
}
