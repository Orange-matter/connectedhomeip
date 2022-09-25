package com.google.chip.chiptool.util

import android.content.SharedPreferences
import androidx.core.content.edit


abstract class Prefs<T>(val prefs: SharedPreferences, val key: String, val default: T) {
  abstract fun get(): T
  abstract fun set(value: T)
  fun remove() = prefs.edit { remove(key) }
  val isSet get() = prefs.contains(key)
}

class LongPref(prefs: SharedPreferences, key: String, default: Long = 0) : Prefs<Long>(prefs, key, default) {
  override fun get() = prefs.getLong(key, default)
  override fun set(value: Long) = prefs.edit(true) { putLong(key, value) }
}

class StringPref(prefs: SharedPreferences, key: String, default: String = "") : Prefs<String>(prefs, key, default) {
  override fun get() = prefs.getString(key, default) ?: ""
  override fun set(value: String) = prefs.edit(true) { putString(key, value) }
}
