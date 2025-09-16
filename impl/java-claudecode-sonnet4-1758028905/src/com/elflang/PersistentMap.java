package com.elflang;

import java.util.*;

public class PersistentMap<K, V> implements Iterable<Map.Entry<K, V>> {
    private static final int HASH_BITS = 5;
    private static final int HASH_MASK = (1 << HASH_BITS) - 1;
    private static final Object EMPTY_MARKER = new Object();

    private final Node<K, V> root;
    private final int count;

    private static final PersistentMap<?, ?> EMPTY = new PersistentMap<>(null, 0);

    private PersistentMap(Node<K, V> root, int count) {
        this.root = root;
        this.count = count;
    }

    @SuppressWarnings("unchecked")
    public static <K, V> PersistentMap<K, V> empty() {
        return (PersistentMap<K, V>) EMPTY;
    }

    public static <K, V> PersistentMap<K, V> of(Map<K, V> entries) {
        PersistentMap<K, V> result = empty();
        for (Map.Entry<K, V> entry : entries.entrySet()) {
            result = result.assoc(entry.getKey(), entry.getValue());
        }
        return result;
    }

    public PersistentMap<K, V> assoc(K key, V value) {
        if (key instanceof PersistentMap) {
            throw new RuntimeException("Unable to use a Dictionary as a Dictionary key");
        }

        return assocInternal(key, value);
    }

    // Internal assoc method that allows dictionaries (for use by PersistentSet)
    PersistentMap<K, V> assocAllowDict(K key, V value) {
        return assocInternal(key, value);
    }

    private PersistentMap<K, V> assocInternal(K key, V value) {
        int hash = key == null ? 0 : key.hashCode();
        Node<K, V> newRoot = root == null ? new BitmapNode<>() : root;
        Node<K, V> result = newRoot.assoc(0, hash, key, value);

        if (result == root) {
            return this;
        }

        boolean added = result != root && (root == null || !containsKey(key));
        return new PersistentMap<>(result, added ? count + 1 : count);
    }

    public PersistentMap<K, V> merge(PersistentMap<K, V> other) {
        PersistentMap<K, V> result = this;
        for (Map.Entry<K, V> entry : other) {
            result = result.assoc(entry.getKey(), entry.getValue());
        }
        return result;
    }

    public V get(K key) {
        if (root == null) return null;
        int hash = key == null ? 0 : key.hashCode();
        return root.find(0, hash, key);
    }

    public boolean containsKey(K key) {
        return get(key) != null || (root != null && root.find(0, key == null ? 0 : key.hashCode(), key) != null);
    }

    public int size() {
        return count;
    }

    public boolean isEmpty() {
        return count == 0;
    }

    @Override
    public Iterator<Map.Entry<K, V>> iterator() {
        return new MapIterator();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof PersistentMap)) return false;
        PersistentMap<?, ?> other = (PersistentMap<?, ?>) obj;
        if (count != other.count) return false;

        for (Map.Entry<K, V> entry : this) {
            @SuppressWarnings("unchecked")
            Object otherValue = ((PersistentMap<Object, Object>) other).get(entry.getKey());
            if (!Objects.equals(entry.getValue(), otherValue)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public int hashCode() {
        int hash = 0;
        for (Map.Entry<K, V> entry : this) {
            hash += Objects.hashCode(entry.getKey()) ^ Objects.hashCode(entry.getValue());
        }
        return hash;
    }

    public Map<K, V> toMap() {
        Map<K, V> result = new LinkedHashMap<>();
        for (Map.Entry<K, V> entry : this) {
            result.put(entry.getKey(), entry.getValue());
        }
        return result;
    }

    public List<Map.Entry<K, V>> getSortedEntries() {
        List<Map.Entry<K, V>> sorted = new ArrayList<>();
        for (Map.Entry<K, V> entry : this) {
            sorted.add(entry);
        }
        sorted.sort((a, b) -> compareValues(a.getKey(), b.getKey()));
        return sorted;
    }

    @SuppressWarnings("unchecked")
    private int compareValues(K a, K b) {
        if (a == null && b == null) return 0;
        if (a == null) return -1;
        if (b == null) return 1;

        String aType = a.getClass().getSimpleName();
        String bType = b.getClass().getSimpleName();
        int typeCompare = aType.compareTo(bType);
        if (typeCompare != 0) return typeCompare;

        if (a instanceof Comparable && b instanceof Comparable) {
            return ((Comparable) a).compareTo(b);
        }

        return a.toString().compareTo(b.toString());
    }

    private abstract static class Node<K, V> {
        abstract Node<K, V> assoc(int shift, int hash, K key, V value);
        abstract V find(int shift, int hash, K key);
        abstract void collectEntries(List<Map.Entry<K, V>> entries);
    }

    private static class BitmapNode<K, V> extends Node<K, V> {
        private final int bitmap;
        private final Object[] array;

        BitmapNode() {
            this.bitmap = 0;
            this.array = new Object[0];
        }

        BitmapNode(int bitmap, Object[] array) {
            this.bitmap = bitmap;
            this.array = array;
        }

        @Override
        Node<K, V> assoc(int shift, int hash, K key, V value) {
            int bit = 1 << ((hash >>> shift) & HASH_MASK);
            int index = Integer.bitCount(bitmap & (bit - 1));

            if ((bitmap & bit) != 0) {
                int keyIndex = 2 * index;
                int valueIndex = keyIndex + 1;

                if (Objects.equals(array[keyIndex], key)) {
                    if (Objects.equals(array[valueIndex], value)) {
                        return this;
                    }
                    Object[] newArray = array.clone();
                    newArray[valueIndex] = value;
                    return new BitmapNode<>(bitmap, newArray);
                }

                if (shift + HASH_BITS < 32) {
                    Node<K, V> subNode = new BitmapNode<K, V>().assoc(shift + HASH_BITS,
                        array[keyIndex] == null ? 0 : array[keyIndex].hashCode(),
                        (K) array[keyIndex], (V) array[valueIndex]);
                    subNode = subNode.assoc(shift + HASH_BITS, hash, key, value);

                    Object[] newArray = new Object[array.length];
                    System.arraycopy(array, 0, newArray, 0, keyIndex);
                    newArray[keyIndex] = subNode;
                    System.arraycopy(array, keyIndex + 2, newArray, keyIndex + 1, array.length - keyIndex - 2);

                    return new BitmapNode<>(bitmap, newArray);
                }

                return this;
            } else {
                Object[] newArray = new Object[array.length + 2];
                int keyIndex = 2 * index;

                System.arraycopy(array, 0, newArray, 0, keyIndex);
                newArray[keyIndex] = key;
                newArray[keyIndex + 1] = value;
                System.arraycopy(array, keyIndex, newArray, keyIndex + 2, array.length - keyIndex);

                return new BitmapNode<>(bitmap | bit, newArray);
            }
        }

        @Override
        @SuppressWarnings("unchecked")
        V find(int shift, int hash, K key) {
            int bit = 1 << ((hash >>> shift) & HASH_MASK);
            if ((bitmap & bit) == 0) {
                return null;
            }

            int index = Integer.bitCount(bitmap & (bit - 1));
            int keyIndex = 2 * index;

            if (keyIndex >= array.length) {
                return null;
            }

            if (array[keyIndex] instanceof Node) {
                return ((Node<K, V>) array[keyIndex]).find(shift + HASH_BITS, hash, key);
            }

            if (Objects.equals(array[keyIndex], key)) {
                return (V) array[keyIndex + 1];
            }

            return null;
        }

        @Override
        @SuppressWarnings("unchecked")
        void collectEntries(List<Map.Entry<K, V>> entries) {
            for (int i = 0; i < array.length; i += 2) {
                if (array[i] instanceof Node) {
                    ((Node<K, V>) array[i]).collectEntries(entries);
                } else {
                    entries.add(new AbstractMap.SimpleEntry<>((K) array[i], (V) array[i + 1]));
                }
            }
        }
    }

    private class MapIterator implements Iterator<Map.Entry<K, V>> {
        private final List<Map.Entry<K, V>> entries;
        private int index = 0;

        MapIterator() {
            this.entries = new ArrayList<>();
            if (root != null) {
                root.collectEntries(entries);
            }
        }

        @Override
        public boolean hasNext() {
            return index < entries.size();
        }

        @Override
        public Map.Entry<K, V> next() {
            if (!hasNext()) {
                throw new NoSuchElementException();
            }
            return entries.get(index++);
        }
    }
}