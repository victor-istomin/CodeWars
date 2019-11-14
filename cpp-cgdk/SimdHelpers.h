#pragma once

#include "geometry.h"
#include "state.h"
#include <vector>

template <typename FieldType, size_t PropertiesCount>
class SimdHelper
{
public:

    struct View
    {
        FieldType* m_begin;
        FieldType* m_end;
    };

    using Value = FieldType;

private:

    using Buffer = std::unique_ptr<FieldType[]>;

    //FieldType m_field[PropertiesCount][MaxObjectsCount];      // e.g. 2 fields, 3 rows { {x,x,x}, {y,y,y} }
    std::array<View,   PropertiesCount> m_properties;
    std::array<Buffer, PropertiesCount> m_holders;

// #fixme:
protected:
    size_t m_objectsCount = 0;

private:
    size_t m_capacity     = 0;


    template <typename Field, typename ... Fields>
    void addCell(size_t objectIndex, size_t cellIndex, Field&& first, Fields&& ... rest)
    {
        getPropertyView(cellIndex).m_begin[objectIndex] = std::forward<Field>(first);

        if constexpr(sizeof ... (rest) > 0)
            addCell(objectIndex, ++cellIndex, std::forward<Fields>(rest)...);
    }

protected:

    View getPropertyView(size_t propertyIndex)               { return m_properties[propertyIndex]; }

    template <typename ... Fields>
    void addRow(Fields&& ... args)
    {
        static_assert(sizeof ... (args) == PropertiesCount, "Insufficient parameters for the whole object");

        addCell(m_objectsCount, 0, std::forward<Fields>(args)...);
        m_objectsCount += 1;
    }

public:

    size_t capacity() const { return m_capacity; }
    size_t size()     const { return m_objectsCount; }

    void clear()            { m_objectsCount = 0; }

    void allocate(size_t count)
    {
        const size_t cacheLineSize = 128;
        count = std::max(count, cacheLineSize / sizeof(FieldType));

        size_t bufferIndex = 0;
        for(Buffer& buffer : m_holders)
        {
            size_t elementsCount = count + cacheLineSize / sizeof(FieldType) + 1;
            buffer.reset(new FieldType[elementsCount]);
            void*  data   = buffer.get();
            size_t rest   = elementsCount * sizeof(FieldType);
            size_t needed = count * sizeof(FieldType);

            m_properties[bufferIndex].m_begin = reinterpret_cast<FieldType*>(std::align(cacheLineSize, needed, data, rest));
            m_properties[bufferIndex].m_end   = m_properties[bufferIndex].m_begin + count;

            if(nullptr == m_properties[bufferIndex].m_begin)
                throw std::bad_alloc();

            ++bufferIndex;
        }

        m_capacity = count;
    }

    void allocateAtLeast(size_t count)
    {
        if(capacity() < count)
            allocate(count);
    }
};

class VehiclePosSimd : public SimdHelper<double, 3/*x,y,extra-parameter*/>
{
    using Base = SimdHelper<float, 3/*x,y,extra*/>;

public:
    void addVehicle(const model::Vehicle& v, float extra = 0)
    {
        addRow(v.getX(), v.getY(), extra);
    }

    void addVehicles(const std::vector<VehiclePtr>& vehicles)
    {
        m_objectsCount = vehicles.size();

        View xs = getXs();
        for(size_t i = 0; i < vehicles.size(); ++i)
            xs.m_begin[i] = vehicles[i]->getX();

        View ys = getYs();
        for(size_t i = 0; i < vehicles.size(); ++i)
            ys.m_begin[i] = vehicles[i]->getY();

        View ranges = getExtras();
        for(size_t i = 0; i < vehicles.size(); ++i)
            ranges.m_begin[i] = vehicles[i]->getSquaredVisionRange();
    }

    View getXs()        { return getPropertyView(0); }
    View getYs()        { return getPropertyView(1); }
    View getExtras()    { return getPropertyView(2); }
};

