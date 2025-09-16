package com.elflang;

import java.util.*;

public class PersistentList<E> implements Iterable<E> {
    private final Node<E> head;
    private final int count;

    private static final PersistentList<?> EMPTY = new PersistentList<>(null, 0);

    private PersistentList(Node<E> head, int count) {
        this.head = head;
        this.count = count;
    }

    @SuppressWarnings("unchecked")
    public static <E> PersistentList<E> empty() {
        return (PersistentList<E>) EMPTY;
    }

    public static <E> PersistentList<E> of(Collection<E> elements) {
        PersistentList<E> result = empty();
        List<E> reversed = new ArrayList<>(elements);
        Collections.reverse(reversed);
        for (E element : reversed) {
            result = result.cons(element);
        }
        return result;
    }

    public static <E> PersistentList<E> of(E... elements) {
        PersistentList<E> result = empty();
        for (int i = elements.length - 1; i >= 0; i--) {
            result = result.cons(elements[i]);
        }
        return result;
    }

    public PersistentList<E> cons(E element) {
        return new PersistentList<>(new Node<>(element, head), count + 1);
    }

    public E first() {
        return head == null ? null : head.value;
    }

    public PersistentList<E> rest() {
        return head == null ? empty() : new PersistentList<>(head.next, count - 1);
    }

    public PersistentList<E> concat(PersistentList<E> other) {
        if (isEmpty()) return other;
        if (other.isEmpty()) return this;

        PersistentList<E> result = other;
        PersistentList<E> current = this;
        List<E> elements = new ArrayList<>();

        while (!current.isEmpty()) {
            elements.add(current.first());
            current = current.rest();
        }

        Collections.reverse(elements);
        for (E element : elements) {
            result = result.cons(element);
        }

        return result;
    }

    public E get(int index) {
        if (index < 0) {
            index = count + index;
        }
        if (index < 0 || index >= count) {
            return null;
        }

        Node<E> current = head;
        for (int i = 0; i < index && current != null; i++) {
            current = current.next;
        }
        return current == null ? null : current.value;
    }

    public int size() {
        return count;
    }

    public boolean isEmpty() {
        return head == null;
    }

    @Override
    public Iterator<E> iterator() {
        return new Iterator<E>() {
            private Node<E> current = head;

            @Override
            public boolean hasNext() {
                return current != null;
            }

            @Override
            public E next() {
                if (!hasNext()) {
                    throw new NoSuchElementException();
                }
                E value = current.value;
                current = current.next;
                return value;
            }
        };
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof PersistentList)) return false;
        PersistentList<?> other = (PersistentList<?>) obj;
        if (count != other.count) return false;

        Node<E> thisCurrent = head;
        Node<?> otherCurrent = other.head;

        while (thisCurrent != null && otherCurrent != null) {
            if (!Objects.equals(thisCurrent.value, otherCurrent.value)) {
                return false;
            }
            thisCurrent = thisCurrent.next;
            otherCurrent = otherCurrent.next;
        }

        return thisCurrent == null && otherCurrent == null;
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

    private static class Node<E> {
        final E value;
        final Node<E> next;

        Node(E value, Node<E> next) {
            this.value = value;
            this.next = next;
        }
    }
}