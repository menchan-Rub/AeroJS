/**
 * @brief このオブジェクトがSetオブジェクトであるか確認します
 * @return Setオブジェクトの場合はtrue
 */
virtual bool isSetObject() const { return false; }

/**
 * @brief このオブジェクトがWeakMapオブジェクトであるか確認します
 * @return WeakMapオブジェクトの場合はtrue
 */
virtual bool isWeakMapObject() const { return false; } 