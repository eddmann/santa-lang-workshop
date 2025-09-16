package com.elflang;

import java.util.*;

public class PersistentSet<E> implements Iterable<E> {
    private final PersistentMap<E, Boolean> impl;

    private PersistentSet(PersistentMap<E, Boolean> impl) {
        this.impl = impl;
    }

    public static <E> PersistentSet<E> empty() {
        return new PersistentSet<>(PersistentMap.empty());
    }

    public static <E> PersistentSet<E> of(Collection<E> elements) {
        PersistentSet<E> result = empty();
        for (E element : elements) {
            result = result.push(element);
        }
        return result;
    }

    public PersistentSet<E> push(E element) {
        if (contains(element)) {
            return this;
        }

        // For sets, we need to handle dictionary elements differently than dictionary keys
        PersistentMap<E, Boolean> newImpl;
        if (element instanceof PersistentMap) {
            // Allow dictionaries in sets via push by bypassing the dictionary key check
            newImpl = impl.assocAllowDict(element, Boolean.TRUE);
        } else {
            newImpl = impl.assoc(element, Boolean.TRUE);
        }
        return new PersistentSet<>(newImpl);
    }

    public PersistentSet<E> union(PersistentSet<E> other) {
        PersistentSet<E> result = this;
        for (E element : other) {
            result = result.push(element);
        }
        return result;
    }

    public boolean contains(E element) {
        return impl.containsKey(element);
    }

    public int size() {
        return impl.size();
    }

    public boolean isEmpty() {
        return impl.isEmpty();
    }

    @Override
    public Iterator<E> iterator() {
        return new Iterator<E>() {
            private final Iterator<Map.Entry<E, Boolean>> implIterator = impl.iterator();

            @Override
            public boolean hasNext() {
                return implIterator.hasNext();
            }

            @Override
            public E next() {
                return implIterator.next().getKey();
            }
        };
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof PersistentSet)) return false;
        PersistentSet<?> other = (PersistentSet<?>) obj;
        if (size() != other.size()) return false;

        for (E element : this) {
            @SuppressWarnings("unchecked")
            PersistentSet<Object> otherSet = (PersistentSet<Object>) other;
            if (!otherSet.contains(element)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public int hashCode() {
        int hash = 0;
        for (E element : this) {
            hash += Objects.hashCode(element);
        }
        return hash;
    }

    public Set<E> toSet() {
        Set<E> result = new LinkedHashSet<>();
        for (E element : this) {
            result.add(element);
        }
        return result;
    }

    public List<E> getSortedElements() {
        List<E> sorted = new ArrayList<>();
        for (E element : this) {
            sorted.add(element);
        }
        sorted.sort((a, b) -> compareValues(a, b));
        return sorted;
    }

    @SuppressWarnings("unchecked")
    private int compareValues(E a, E b) {
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
}