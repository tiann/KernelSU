package me.weishu.kernelsu.ui.component.navigation

import android.util.Log
import androidx.compose.runtime.saveable.Saver

class RoutePopupStack {
    internal val _keyOrder = mutableListOf<String>()
    internal val _state = mutableMapOf<String, Boolean>()

    val keys: List<String> get() = _keyOrder
    val state: Map<String, Boolean> get() = _state

    fun put(key: String, value: Boolean) {
        if (!_state.containsKey(key)) {
            _keyOrder.add(key)
        }
        _state[key] = value
    }

    fun putLast(value: Boolean) {
        if (_keyOrder.isNotEmpty()) {
            _state[_keyOrder.last()] = value
        }
    }

    fun removeLast(): String? {
        if (_keyOrder.isNotEmpty()) {
            val lastKey = _keyOrder.removeAt(_keyOrder.size - 1)
            _state.remove(lastKey)
            return lastKey
        }
        return null
    }

    fun remove(key: String) {
        _keyOrder.remove(key)
        _state.remove(key)
    }

    fun clear(){
        _keyOrder.clear()
        _state.clear()
    }

    fun isEmpty(): Boolean = _keyOrder.isEmpty()
    fun isNotEmpty(): Boolean = _keyOrder.isNotEmpty()
    fun contains(key: String): Boolean = _state.containsKey(key)

    fun getValue(key: String): Boolean {
        return _state.getOrDefault(key, false).also {
            if (!_state.containsKey(key)) {
                Log.d("RoutePopupStack", "$key not found")
            }
        }
    }

    companion object {
        val Saver = Saver<RoutePopupStack, Pair<List<String>, Map<String, Boolean>>>(
            save = { stack ->
                stack.keys to stack.state // ✅ 只用 public getter
            },
            restore = { (keys, stateMap) ->
                RoutePopupStack().apply {
                    _keyOrder.addAll(keys)     // ✅ internal，同模块可写
                    _state.putAll(stateMap)
                }
            }
        )
    }
}