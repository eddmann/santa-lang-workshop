package com.elflang;

import java.util.*;

public class PersistentVector<E> implements Iterable<E> {
    private static final int BRANCH_FACTOR = 32;
    private static final int MASK = BRANCH_FACTOR - 1;
    private static final Object[] EMPTY_ARRAY = new Object[0];

    private final Node<E> root;
    private final Object[] tail;
    private final int count;
    private final int shift;

    private static final PersistentVector<?> EMPTY = new PersistentVector<>(null, EMPTY_ARRAY, 0, 0);

    private PersistentVector(Node<E> root, Object[] tail, int count, int shift) {
        this.root = root;
        this.tail = tail;
        this.count = count;
        this.shift = shift;
    }

    @SuppressWarnings("unchecked")
    public static <E> PersistentVector<E> empty() {
        return (PersistentVector<E>) EMPTY;
    }

    public static <E> PersistentVector<E> of(List<E> elements) {
        PersistentVector<E> result = empty();
        for (E element : elements) {
            result = result.push(element);
        }
        return result;
    }

    public PersistentVector<E> push(E element) {
        int tailLen = count - tailOffset();

        if (tailLen < BRANCH_FACTOR) {
            Object[] newTail = new Object[tailLen + 1];
            System.arraycopy(tail, 0, newTail, 0, tailLen);
            newTail[tailLen] = element;
            return new PersistentVector<>(root, newTail, count + 1, shift);
        }

        Node<E> newRoot = root;
        int newShift = shift;

        Node<E> tailNode = new Node<>(tail);

        if ((count >>> 5) > (1 << shift)) {
            newRoot = new Node<>(new Node[]{root, createPath(shift, tailNode)});
            newShift += 5;
        } else {
            newRoot = pushTail(shift, root, tailNode);
        }

        return new PersistentVector<>(newRoot, new Object[]{element}, count + 1, newShift);
    }

    public PersistentVector<E> concat(PersistentVector<E> other) {
        PersistentVector<E> result = this;
        for (E element : other) {
            result = result.push(element);
        }
        return result;
    }

    @SuppressWarnings("unchecked")
    public E get(int index) {
        if (index < 0) {
            index = count + index;
        }
        if (index < 0 || index >= count) {
            return null;
        }

        if (index >= tailOffset()) {
            return (E) tail[index & MASK];
        }

        return (E) arrayFor(index)[index & MASK];
    }

    public E first() {
        return count == 0 ? null : get(0);
    }

    public PersistentVector<E> rest() {
        if (count <= 1) {
            return empty();
        }

        PersistentVector<E> result = empty();
        for (int i = 1; i < count; i++) {
            result = result.push(get(i));
        }
        return result;
    }

    public int size() {
        return count;
    }

    public boolean isEmpty() {
        return count == 0;
    }

    @Override
    public Iterator<E> iterator() {
        return new Iterator<E>() {
            private int index = 0;

            @Override
            public boolean hasNext() {
                return index < count;
            }

            @Override
            public E next() {
                if (!hasNext()) {
                    throw new NoSuchElementException();
                }
                return get(index++);
            }
        };
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof PersistentVector)) return false;
        PersistentVector<?> other = (PersistentVector<?>) obj;
        if (count != other.count) return false;

        for (int i = 0; i < count; i++) {
            if (!Objects.equals(get(i), other.get(i))) {
                return false;
            }
        }
        return true;
    }

    @Override
    public int hashCode() {
        int hash = 1;
        for (E element : this) {
            hash = 31 * hash + Objects.hashCode(element);
        }
        return hash;
    }

    public List<E> toList() {
        List<E> result = new ArrayList<>(count);
        for (E element : this) {
            result.add(element);
        }
        return result;
    }

    private int tailOffset() {
        return count < BRANCH_FACTOR ? 0 : ((count - 1) >>> 5) << 5;
    }

    private Object[] arrayFor(int index) {
        if (index >= 0 && index < count) {
            if (index >= tailOffset()) {
                return tail;
            }
            Node<E> node = root;
            for (int level = shift; level > 0; level -= 5) {
                node = node.array[(index >>> level) & MASK];
            }
            return node.array;
        }
        throw new IndexOutOfBoundsException();
    }

    private Node<E> pushTail(int level, Node<E> parent, Node<E> tailNode) {
        int subIndex = ((count - 1) >>> level) & MASK;
        Node<E> result = new Node<>(parent.array.clone());
        Node<E> nodeToInsert;

        if (level == 5) {
            nodeToInsert = tailNode;
        } else {
            Node<E> child = result.array.length > subIndex ? result.array[subIndex] : null;
            nodeToInsert = (child != null) ? pushTail(level - 5, child, tailNode) : createPath(level - 5, tailNode);
        }

        if (result.array.length <= subIndex) {
            Node<E>[] newArray = Arrays.copyOf(result.array, subIndex + 1);
            newArray[subIndex] = nodeToInsert;
            return new Node<>(newArray);
        }

        result.array[subIndex] = nodeToInsert;
        return result;
    }

    private Node<E> createPath(int level, Node<E> node) {
        if (level == 0) {
            return node;
        }
        return new Node<>(new Node[]{createPath(level - 5, node)});
    }

    private static class Node<E> {
        final Node<E>[] array;

        @SuppressWarnings("unchecked")
        Node(Object[] array) {
            this.array = (Node<E>[]) array;
        }

        @SuppressWarnings("unchecked")
        Node(Node<E>[] array) {
            this.array = array;
        }
    }
}