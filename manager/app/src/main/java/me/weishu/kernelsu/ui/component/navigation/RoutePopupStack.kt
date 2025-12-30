package me.weishu.kernelsu.ui.component.navigation

import android.util.Log

class RoutePopupStack {
    private val _keyOrder = mutableListOf<String>()
    private val _state = mutableMapOf<String, Boolean>()

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
        _keyOrder.remove(key) // O(n)，但通常列表很短
        _state.remove(key)
    }

    fun isEmpty(): Boolean = _keyOrder.isEmpty()
    fun isNotEmpty(): Boolean = _keyOrder.isNotEmpty()

    fun contains(key: String): Boolean = _state.containsKey(key)

    fun getValue(key: String): Boolean = _state[key] ?: run{ Log.d("RoutePopupStack", " $key null")
        return false }
}