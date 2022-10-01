// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.


// This file defines an internal static class used to throw exceptions in BCL code.
// The main purpose is to reduce code size.
//
// The old way to throw an exception generates quite a lot IL code and assembly code.
// Following is an example:
//     C# source
//          throw new ArgumentNullException(nameof(key), SR.ArgumentNull_Key);
//     IL code:
//          IL_0003:  ldstr      "key"
//          IL_0008:  ldstr      "ArgumentNull_Key"
//          IL_000d:  call       string System.Environment::GetResourceString(string)
//          IL_0012:  newobj     instance void System.ArgumentNullException::.ctor(string,string)
//          IL_0017:  throw
//    which is 21bytes in IL.
//
// So we want to get rid of the ldstr and call to Environment.GetResource in IL.
// In order to do that, I created two enums: ExceptionResource, ExceptionArgument to represent the
// argument name and resource name in a small integer. The source code will be changed to
//    ThrowHelper.ThrowArgumentNullException(ExceptionArgument.key, ExceptionResource.ArgumentNull_Key);
//
// The IL code will be 7 bytes.
//    IL_0008:  ldc.i4.4
//    IL_0009:  ldc.i4.4
//    IL_000a:  call       void System.ThrowHelper::ThrowArgumentNullException(valuetype System.ExceptionArgument)
//    IL_000f:  ldarg.0
//
// This will also reduce the Jitted code size a lot.
//
// It is very important we do this for generic classes because we can easily generate the same code
// multiple times for different instantiation.
//

#nullable enable
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.CompilerServices;

namespace System
{
    [StackTraceHidden]
    internal static class ThrowHelper
    {
        // internal static void ThrowArrayTypeMismatchException()
        // {
        //     throw new ArrayTypeMismatchException();
        // }

        // internal static void ThrowInvalidTypeWithPointersNotSupported(Type targetType)
        // {
        //     throw new ArgumentException(SR.Format(SR.Argument_InvalidTypeWithPointersNotSupported, targetType));
        // }

        internal static void ThrowIndexOutOfRangeException()
        {
            throw new IndexOutOfRangeException();
        }

        internal static void ThrowArgumentOutOfRangeException()
        {
            throw new ArgumentOutOfRangeException();
        }

        internal static void ThrowArgumentException_DestinationTooShort()
        {
            throw new ArgumentException("Destination is too short.", "destination");
        }

        internal static void ThrowArgumentException_OverlapAlignmentMismatch()
        {
            throw new ArgumentException("Overlapping spans have mismatching alignment.");
        }

        // internal static void ThrowArgumentException_ArgumentNull_TypedRefType()
        // {
        //     throw new ArgumentNullException("value", SR.ArgumentNull_TypedRefType);
        // }

        internal static void ThrowArgumentException_CannotExtractScalar(ExceptionArgument argument)
        {
            throw GetArgumentException(ExceptionResource.Argument_CannotExtractScalar, argument);
        }

        // internal static void ThrowArgumentException_TupleIncorrectType(object obj)
        // {
        //     throw new ArgumentException(SR.Format(SR.ArgumentException_ValueTupleIncorrectType, obj.GetType()), "other");
        // }

        internal static void ThrowArgumentOutOfRange_IndexMustBeLessException()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.index,
                                                    ExceptionResource.ArgumentOutOfRange_IndexMustBeLess);
        }

        internal static void ThrowArgumentOutOfRange_IndexMustBeLessOrEqualException()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.index,
                                                    ExceptionResource.ArgumentOutOfRange_IndexMustBeLessOrEqual);
        }

        internal static void ThrowArgumentOutOfRange_IndexException()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.index,
                                                    ExceptionResource.ArgumentOutOfRange_Index);
        }
        internal static void ThrowArgumentException_BadComparer(object comparer)
        {
            throw new ArgumentException( $"Unable to sort because the IComparer.Compare() method returns inconsistent results. Either a value does not compare equal to itself, or one value repeatedly compared to another value yields different results. IComparer: '{string.Concat(comparer)}'.");
        }

        internal static void ThrowIndexArgumentOutOfRange_NeedNonNegNumException()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.index,
                                                    ExceptionResource.ArgumentOutOfRange_NeedNonNegNum);
        }

        internal static void ThrowValueArgumentOutOfRange_NeedNonNegNumException()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.value,
                                                    ExceptionResource.ArgumentOutOfRange_NeedNonNegNum);
        }

        internal static void ThrowLengthArgumentOutOfRange_ArgumentOutOfRange_NeedNonNegNum()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.length,
                                                    ExceptionResource.ArgumentOutOfRange_NeedNonNegNum);
        }

        internal static void ThrowStartIndexArgumentOutOfRange_ArgumentOutOfRange_IndexMustBeLessOrEqual()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.startIndex,
                                                    ExceptionResource.ArgumentOutOfRange_IndexMustBeLessOrEqual);
        }

        internal static void ThrowStartIndexArgumentOutOfRange_ArgumentOutOfRange_IndexMustBeLess()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.startIndex,
                                                    ExceptionResource.ArgumentOutOfRange_IndexMustBeLess);
        }

        internal static void ThrowCountArgumentOutOfRange_ArgumentOutOfRange_Count()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.count,
                                                    ExceptionResource.ArgumentOutOfRange_Count);
        }

        internal static void ThrowArgumentOutOfRange_Year()
        {
            throw GetArgumentOutOfRangeException(ExceptionArgument.year,
                                                    ExceptionResource.ArgumentOutOfRange_Year);
        }

        internal static void ThrowArgumentOutOfRange_Month(int month)
        {
            throw new ArgumentOutOfRangeException(nameof(month), month, "Month must be between one and twelve.");
        }

        internal static void ThrowArgumentOutOfRange_DayNumber(int dayNumber)
        {
            throw new ArgumentOutOfRangeException(nameof(dayNumber), dayNumber, "Day number must be between 0 and DateOnly.MaxValue.DayNumber.");
        }

        internal static void ThrowArgumentOutOfRange_BadYearMonthDay()
        {
            throw new ArgumentOutOfRangeException(null, "Year, Month, and Day parameters describe an un-representable DateTime.");
        }

        internal static void ThrowArgumentOutOfRange_BadHourMinuteSecond()
        {
            throw new ArgumentOutOfRangeException(null, "Hour, Minute, and Second parameters describe an un-representable DateTime.");
        }

        internal static void ThrowArgumentOutOfRange_TimeSpanTooLong()
        {
            throw new ArgumentOutOfRangeException(null, "TimeSpan overflowed because the duration is too long.");
        }

        internal static void ThrowOverflowException()
        {
            throw new OverflowException();
        }

        internal static void ThrowOverflowException_TimeSpanTooLong()
        {
            throw new OverflowException("TimeSpan overflowed because the duration is too long.");
        }

        internal static void ThrowArgumentException_Arg_CannotBeNaN()
        {
            throw new ArgumentException("TimeSpan does not accept floating point Not-a-Number values.");
        }

        // internal static void ThrowWrongKeyTypeArgumentException<T>(T key, Type targetType)
        // {
        //     // Generic key to move the boxing to the right hand side of throw
        //     throw GetWrongKeyTypeArgumentException(key, targetType);
        // }
        //
        // internal static void ThrowWrongValueTypeArgumentException<T>(T value, Type targetType)
        // {
        //     // Generic key to move the boxing to the right hand side of throw
        //     throw GetWrongValueTypeArgumentException((object?)value, targetType);
        // }

        private static ArgumentException GetAddingDuplicateWithKeyArgumentException(object? key)
        {
            return new ArgumentException($"An item with the same key has already been added. Key: {string.Concat(key)}");
        }

        internal static void ThrowAddingDuplicateWithKeyArgumentException<T>(T key)
        {
            // Generic key to move the boxing to the right hand side of throw
            throw GetAddingDuplicateWithKeyArgumentException((object?)key);
        }

        internal static void ThrowKeyNotFoundException<T>(T key)
        {
            // Generic key to move the boxing to the right hand side of throw
            throw GetKeyNotFoundException((object?)key);
        }

        internal static void ThrowArgumentException(ExceptionResource resource)
        {
            throw GetArgumentException(resource);
        }

        internal static void ThrowArgumentException(ExceptionResource resource, ExceptionArgument argument)
        {
            throw GetArgumentException(resource, argument);
        }
        
        internal static void ThrowArgumentNullException(ExceptionArgument argument)
        {
            throw new ArgumentNullException(GetArgumentName(argument));
        }

        internal static void ThrowArgumentNullException(ExceptionResource resource)
        {
            throw new ArgumentNullException(GetResourceString(resource));
        }

        internal static void ThrowArgumentNullException(ExceptionArgument argument, ExceptionResource resource)
        {
            throw new ArgumentNullException(GetArgumentName(argument), GetResourceString(resource));
        }

        internal static void ThrowArgumentOutOfRangeException(ExceptionArgument argument)
        {
            throw new ArgumentOutOfRangeException(GetArgumentName(argument));
        }

        internal static void ThrowArgumentOutOfRangeException(ExceptionArgument argument, ExceptionResource resource)
        {
            throw GetArgumentOutOfRangeException(argument, resource);
        }

        internal static void ThrowArgumentOutOfRangeException(ExceptionArgument argument, int paramNumber, ExceptionResource resource)
        {
            throw GetArgumentOutOfRangeException(argument, paramNumber, resource);
        }

        // internal static void ThrowEndOfFileException()
        // {
        //     throw CreateEndOfFileException();
        // }

        // internal static Exception CreateEndOfFileException() =>
        //     new EndOfStreamException(SR.IO_EOF_ReadBeyondEOF);

        internal static void ThrowInvalidOperationException()
        {
            throw new InvalidOperationException();
        }

        internal static void ThrowInvalidOperationException(ExceptionResource resource)
        {
            throw GetInvalidOperationException(resource);
        }

        internal static void ThrowInvalidOperationException(ExceptionResource resource, Exception e)
        {
            throw new InvalidOperationException(GetResourceString(resource), e);
        }

        internal static void ThrowNullReferenceException()
        {
            throw new NullReferenceException("The method was called with a null array argument.");
        }

        // internal static void ThrowSerializationException(ExceptionResource resource)
        // {
        //     throw new SerializationException(GetResourceString(resource));
        // }

        // internal static void ThrowRankException(ExceptionResource resource)
        // {
        //     throw new RankException(GetResourceString(resource));
        // }

        internal static void ThrowNotSupportedException(ExceptionResource resource)
        {
            throw new NotSupportedException(GetResourceString(resource));
        }

        internal static void ThrowNotSupportedException_UnseekableStream()
        {
            throw new NotSupportedException("Stream does not support seeking.");
        }

        internal static void ThrowNotSupportedException_UnreadableStream()
        {
            throw new NotSupportedException("Stream does not support reading.");
        }

        internal static void ThrowNotSupportedException_UnwritableStream()
        {
            throw new NotSupportedException("Stream does not support writing.");
        }

        internal static void ThrowObjectDisposedException(object? instance)
        {
            throw new ObjectDisposedException(instance?.GetType().FullName);
        }

        internal static void ThrowObjectDisposedException(Type? type)
        {
            throw new ObjectDisposedException(type?.FullName);
        }

        internal static void ThrowObjectDisposedException_StreamClosed(string? objectName)
        {
            throw new ObjectDisposedException(objectName, "Cannot access a closed stream.");
        }

        internal static void ThrowObjectDisposedException_FileClosed()
        {
            throw new ObjectDisposedException(null, "Cannot access a closed file.");
        }

        internal static void ThrowObjectDisposedException(ExceptionResource resource)
        {
            throw new ObjectDisposedException(null, GetResourceString(resource));
        }

        internal static void ThrowNotSupportedException()
        {
            throw new NotSupportedException();
        }

        // internal static void ThrowAggregateException(List<Exception> exceptions)
        // {
        //     throw new AggregateException(exceptions);
        // }

        internal static void ThrowOutOfMemoryException()
        {
            throw new OutOfMemoryException();
        }

        internal static void ThrowArgumentException_InvalidHandle(string? paramName)
        {
            throw new ArgumentException("Invalid handle.", paramName);
        }

        internal static void ThrowInvalidOperationException_InvalidOperation_EnumNotStarted()
        {
            throw new InvalidOperationException("Enumeration has not started. Call MoveNext.");
        }

        internal static void ThrowInvalidOperationException_InvalidOperation_EnumEnded()
        {
            throw new InvalidOperationException("Enumeration already finished.");
        }

        internal static void ThrowInvalidOperationException_EnumCurrent(int index)
        {
            throw GetInvalidOperationException_EnumCurrent(index);
        }

        internal static void ThrowInvalidOperationException_InvalidOperation_EnumFailedVersion()
        {
            throw new InvalidOperationException("Collection was modified; enumeration operation may not execute.");
        }

        internal static void ThrowInvalidOperationException_InvalidOperation_EnumOpCantHappen()
        {
            throw new InvalidOperationException("Enumeration has either not started or has already finished.");
        }

        internal static void ThrowInvalidOperationException_InvalidOperation_NoValue()
        {
            throw new InvalidOperationException("Nullable object must have a value.");
        }

        internal static void ThrowInvalidOperationException_ConcurrentOperationsNotSupported()
        {
            throw new InvalidOperationException("Operations that change non-concurrent collections must have exclusive access. A concurrent update was performed on this collection and corrupted its state. The collection's state is no longer correct.");
        }

        internal static void ThrowInvalidOperationException_HandleIsNotInitialized()
        {
            throw new InvalidOperationException("Handle is not initialized.");
        }

        // internal static void ThrowInvalidOperationException_HandleIsNotPinned()
        // {
        //     throw new InvalidOperationException(SR.InvalidOperation_HandleIsNotPinned);
        // }

        internal static void ThrowArraySegmentCtorValidationFailedExceptions(Array? array, int offset, int count)
        {
            throw GetArraySegmentCtorValidationFailedException(array, offset, count);
        }

        // internal static void ThrowFormatException_BadFormatSpecifier()
        // {
        //     throw new FormatException(SR.Argument_BadFormatSpecifier);
        // }

        // internal static void ThrowArgumentOutOfRangeException_PrecisionTooLarge()
        // {
        //     throw new ArgumentOutOfRangeException("precision", SR.Format(SR.Argument_PrecisionTooLarge, StandardFormat.MaxPrecision));
        // }
        //
        // internal static void ThrowArgumentOutOfRangeException_SymbolDoesNotFit()
        // {
        //     throw new ArgumentOutOfRangeException("symbol", SR.Argument_BadFormatSpecifier);
        // }

        internal static void ThrowArgumentOutOfRangeException_NeedNonNegNum(string paramName)
        {
            throw new ArgumentOutOfRangeException(paramName, "Non-negative number required.");
        }

        internal static void ArgumentOutOfRangeException_Enum_Value()
        {
            throw new ArgumentOutOfRangeException("value", "Enum value was out of legal range.");
        }

        // internal static void ThrowApplicationException(int hr)
        // {
        //     // Get a message for this HR
        //     Exception? ex = Marshal.GetExceptionForHR(hr);
        //     if (ex != null && !string.IsNullOrEmpty(ex.Message))
        //     {
        //         ex = new ApplicationException(ex.Message);
        //     }
        //     else
        //     {
        //         ex = new ApplicationException();
        //     }
        //
        //     ex.HResult = hr;
        //     throw ex;
        // }

        // internal static void ThrowFormatInvalidString()
        // {
        //     throw new FormatException(SR.Format_InvalidString);
        // }

        private static Exception GetArraySegmentCtorValidationFailedException(Array? array, int offset, int count)
        {
            if (array == null)
                return new ArgumentNullException(nameof(array));
            if (offset < 0)
                return new ArgumentOutOfRangeException(nameof(offset), "Non-negative number required.");
            if (count < 0)
                return new ArgumentOutOfRangeException(nameof(count), "Non-negative number required.");

            // TODO: Debug.Assert(array.Length - offset < count);
            return new ArgumentException("Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection.");
        }

        private static ArgumentException GetArgumentException(ExceptionResource resource)
        {
            return new ArgumentException(GetResourceString(resource));
        }

        private static InvalidOperationException GetInvalidOperationException(ExceptionResource resource)
        {
            return new InvalidOperationException(GetResourceString(resource));
        }

        // private static ArgumentException GetWrongKeyTypeArgumentException(object? key, Type targetType)
        // {
        //     return new ArgumentException(SR.Format(SR.Arg_WrongType, key, targetType), nameof(key));
        // }
        //
        // private static ArgumentException GetWrongValueTypeArgumentException(object? value, Type targetType)
        // {
        //     return new ArgumentException(SR.Format(SR.Arg_WrongType, value, targetType), nameof(value));
        // }

        private static KeyNotFoundException GetKeyNotFoundException(object? key)
        {
            return new KeyNotFoundException($"The given key '{key}' was not present in the dictionary.");
        }

        private static ArgumentOutOfRangeException GetArgumentOutOfRangeException(ExceptionArgument argument, ExceptionResource resource)
        {
            return new ArgumentOutOfRangeException(GetArgumentName(argument), GetResourceString(resource));
        }

        private static ArgumentException GetArgumentException(ExceptionResource resource, ExceptionArgument argument)
        {
            return new ArgumentException(GetResourceString(resource), GetArgumentName(argument));
        }

        private static ArgumentOutOfRangeException GetArgumentOutOfRangeException(ExceptionArgument argument, int paramNumber, ExceptionResource resource)
        {
            return new ArgumentOutOfRangeException(GetArgumentName(argument) + "[" + ((object)paramNumber).ToString() + "]", GetResourceString(resource));
        }

        private static InvalidOperationException GetInvalidOperationException_EnumCurrent(int index)
        {
            return new InvalidOperationException(
                index < 0 ?
                "Enumeration has not started. Call MoveNext." :
                "Enumeration already finished.");
        }

        // Allow nulls for reference types and Nullable<U>, but not for value types.
        // Aggressively inline so the jit evaluates the if in place and either drops the call altogether
        // Or just leaves null test and call to the Non-returning ThrowHelper.ThrowArgumentNullException
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        internal static void IfNullAndNullsAreIllegalThenThrow<T>(object? value, ExceptionArgument argName)
        {
            // Note that default(T) is not equal to null for value types except when T is Nullable<U>.
            if (!(default(T) == null) && value == null)
                ThrowHelper.ThrowArgumentNullException(argName);
        }

        // // Throws if 'T' is disallowed in Vector<T> in the Numerics namespace.
        // // If 'T' is allowed, no-ops. JIT will elide the method entirely if 'T'
        // // is supported and we're on an optimized release build.
        // [MethodImpl(MethodImplOptions.AggressiveInlining)]
        // internal static void ThrowForUnsupportedNumericsVectorBaseType<T>() where T : struct
        // {
        //     if (!Vector<T>.IsSupported)
        //     {
        //         ThrowNotSupportedException(ExceptionResource.Arg_TypeNotSupported);
        //     }
        // }
        //
        // // Throws if 'T' is disallowed in Vector64<T> in the Intrinsics namespace.
        // // If 'T' is allowed, no-ops. JIT will elide the method entirely if 'T'
        // // is supported and we're on an optimized release build.
        // [MethodImpl(MethodImplOptions.AggressiveInlining)]
        // internal static void ThrowForUnsupportedIntrinsicsVector64BaseType<T>() where T : struct
        // {
        //     if (!Vector64<T>.IsSupported)
        //     {
        //         ThrowNotSupportedException(ExceptionResource.Arg_TypeNotSupported);
        //     }
        // }
        //
        // // Throws if 'T' is disallowed in Vector128<T> in the Intrinsics namespace.
        // // If 'T' is allowed, no-ops. JIT will elide the method entirely if 'T'
        // // is supported and we're on an optimized release build.
        // [MethodImpl(MethodImplOptions.AggressiveInlining)]
        // internal static void ThrowForUnsupportedIntrinsicsVector128BaseType<T>() where T : struct
        // {
        //     if (!Vector128<T>.IsSupported)
        //     {
        //         ThrowNotSupportedException(ExceptionResource.Arg_TypeNotSupported);
        //     }
        // }
        //
        // // Throws if 'T' is disallowed in Vector256<T> in the Intrinsics namespace.
        // // If 'T' is allowed, no-ops. JIT will elide the method entirely if 'T'
        // // is supported and we're on an optimized release build.
        // [MethodImpl(MethodImplOptions.AggressiveInlining)]
        // internal static void ThrowForUnsupportedIntrinsicsVector256BaseType<T>() where T : struct
        // {
        //     if (!Vector256<T>.IsSupported)
        //     {
        //         ThrowNotSupportedException(ExceptionResource.Arg_TypeNotSupported);
        //     }
        // }

#if false // Reflection-based implementation does not work for NativeAOT
        // This function will convert an ExceptionArgument enum value to the argument name string.
        [MethodImpl(MethodImplOptions.NoInlining)]
        private static string GetArgumentName(ExceptionArgument argument)
        {
            Debug.Assert(Enum.IsDefined(typeof(ExceptionArgument), argument),
                "The enum value is not defined, please check the ExceptionArgument Enum.");

            return argument.ToString();
        }
#endif

        private static string GetArgumentName(ExceptionArgument argument)
        {
            switch (argument)
            {
                case ExceptionArgument.obj:
                    return "obj";
                case ExceptionArgument.dictionary:
                    return "dictionary";
                case ExceptionArgument.array:
                    return "array";
                case ExceptionArgument.info:
                    return "info";
                case ExceptionArgument.key:
                    return "key";
                case ExceptionArgument.text:
                    return "text";
                case ExceptionArgument.values:
                    return "values";
                case ExceptionArgument.value:
                    return "value";
                case ExceptionArgument.startIndex:
                    return "startIndex";
                case ExceptionArgument.task:
                    return "task";
                case ExceptionArgument.bytes:
                    return "bytes";
                case ExceptionArgument.byteIndex:
                    return "byteIndex";
                case ExceptionArgument.byteCount:
                    return "byteCount";
                case ExceptionArgument.ch:
                    return "ch";
                case ExceptionArgument.chars:
                    return "chars";
                case ExceptionArgument.charIndex:
                    return "charIndex";
                case ExceptionArgument.charCount:
                    return "charCount";
                case ExceptionArgument.s:
                    return "s";
                case ExceptionArgument.input:
                    return "input";
                case ExceptionArgument.ownedMemory:
                    return "ownedMemory";
                case ExceptionArgument.list:
                    return "list";
                case ExceptionArgument.index:
                    return "index";
                case ExceptionArgument.capacity:
                    return "capacity";
                case ExceptionArgument.collection:
                    return "collection";
                case ExceptionArgument.item:
                    return "item";
                case ExceptionArgument.converter:
                    return "converter";
                case ExceptionArgument.match:
                    return "match";
                case ExceptionArgument.count:
                    return "count";
                case ExceptionArgument.action:
                    return "action";
                case ExceptionArgument.comparison:
                    return "comparison";
                case ExceptionArgument.exceptions:
                    return "exceptions";
                case ExceptionArgument.exception:
                    return "exception";
                case ExceptionArgument.pointer:
                    return "pointer";
                case ExceptionArgument.start:
                    return "start";
                case ExceptionArgument.format:
                    return "format";
                case ExceptionArgument.formats:
                    return "formats";
                case ExceptionArgument.culture:
                    return "culture";
                case ExceptionArgument.comparer:
                    return "comparer";
                case ExceptionArgument.comparable:
                    return "comparable";
                case ExceptionArgument.source:
                    return "source";
                case ExceptionArgument.state:
                    return "state";
                case ExceptionArgument.length:
                    return "length";
                case ExceptionArgument.comparisonType:
                    return "comparisonType";
                case ExceptionArgument.manager:
                    return "manager";
                case ExceptionArgument.sourceBytesToCopy:
                    return "sourceBytesToCopy";
                case ExceptionArgument.callBack:
                    return "callBack";
                case ExceptionArgument.creationOptions:
                    return "creationOptions";
                case ExceptionArgument.function:
                    return "function";
                case ExceptionArgument.scheduler:
                    return "scheduler";
                case ExceptionArgument.continuationAction:
                    return "continuationAction";
                case ExceptionArgument.continuationFunction:
                    return "continuationFunction";
                case ExceptionArgument.tasks:
                    return "tasks";
                case ExceptionArgument.asyncResult:
                    return "asyncResult";
                case ExceptionArgument.beginMethod:
                    return "beginMethod";
                case ExceptionArgument.endMethod:
                    return "endMethod";
                case ExceptionArgument.endFunction:
                    return "endFunction";
                case ExceptionArgument.cancellationToken:
                    return "cancellationToken";
                case ExceptionArgument.continuationOptions:
                    return "continuationOptions";
                case ExceptionArgument.delay:
                    return "delay";
                case ExceptionArgument.millisecondsDelay:
                    return "millisecondsDelay";
                case ExceptionArgument.millisecondsTimeout:
                    return "millisecondsTimeout";
                case ExceptionArgument.stateMachine:
                    return "stateMachine";
                case ExceptionArgument.timeout:
                    return "timeout";
                case ExceptionArgument.type:
                    return "type";
                case ExceptionArgument.sourceIndex:
                    return "sourceIndex";
                case ExceptionArgument.sourceArray:
                    return "sourceArray";
                case ExceptionArgument.destinationIndex:
                    return "destinationIndex";
                case ExceptionArgument.destinationArray:
                    return "destinationArray";
                case ExceptionArgument.pHandle:
                    return "pHandle";
                case ExceptionArgument.handle:
                    return "handle";
                case ExceptionArgument.other:
                    return "other";
                case ExceptionArgument.newSize:
                    return "newSize";
                case ExceptionArgument.lowerBounds:
                    return "lowerBounds";
                case ExceptionArgument.lengths:
                    return "lengths";
                case ExceptionArgument.len:
                    return "len";
                case ExceptionArgument.keys:
                    return "keys";
                case ExceptionArgument.indices:
                    return "indices";
                case ExceptionArgument.index1:
                    return "index1";
                case ExceptionArgument.index2:
                    return "index2";
                case ExceptionArgument.index3:
                    return "index3";
                case ExceptionArgument.length1:
                    return "length1";
                case ExceptionArgument.length2:
                    return "length2";
                case ExceptionArgument.length3:
                    return "length3";
                case ExceptionArgument.endIndex:
                    return "endIndex";
                case ExceptionArgument.elementType:
                    return "elementType";
                case ExceptionArgument.arrayIndex:
                    return "arrayIndex";
                case ExceptionArgument.year:
                    return "year";
                case ExceptionArgument.codePoint:
                    return "codePoint";
                case ExceptionArgument.str:
                    return "str";
                case ExceptionArgument.options:
                    return "options";
                case ExceptionArgument.prefix:
                    return "prefix";
                case ExceptionArgument.suffix:
                    return "suffix";
                case ExceptionArgument.buffer:
                    return "buffer";
                case ExceptionArgument.buffers:
                    return "buffers";
                case ExceptionArgument.offset:
                    return "offset";
                case ExceptionArgument.stream:
                    return "stream";
                case ExceptionArgument.anyOf:
                    return "anyOf";
                case ExceptionArgument.overlapped:
                    return "overlapped";
                case ExceptionArgument.minimumBytes:
                    return "minimumBytes";
                default:
                    // TODO: Debug.Fail("The enum value is not defined, please check the ExceptionArgument Enum.");
                    return "";
            }
        }

#if false // Reflection-based implementation does not work for NativeAOT
        // This function will convert an ExceptionResource enum value to the resource string.
        [MethodImpl(MethodImplOptions.NoInlining)]
        private static string GetResourceString(ExceptionResource resource)
        {
            Debug.Assert(Enum.IsDefined(typeof(ExceptionResource), resource),
                "The enum value is not defined, please check the ExceptionResource Enum.");

            return SR.GetResourceString(resource.ToString());
        }
#endif

        private static string GetResourceString(ExceptionResource resource)
        {
            switch (resource)
            {
                case ExceptionResource.ArgumentOutOfRange_Index:
                    return "Index was out of range. Must be non-negative and less than the size of the collection.";
                case ExceptionResource.ArgumentOutOfRange_IndexMustBeLessOrEqual:
                    return "Index was out of range. Must be non-negative and less than or equal to the size of the collection.";
                case ExceptionResource.ArgumentOutOfRange_IndexMustBeLess:
                    return "Index was out of range. Must be non-negative and less than the size of the collection.";
                case ExceptionResource.ArgumentOutOfRange_IndexCount:
                    return "Index and count must refer to a location within the string.";
                case ExceptionResource.ArgumentOutOfRange_IndexCountBuffer:
                    return "Index and count must refer to a location within the buffer.";
                case ExceptionResource.ArgumentOutOfRange_Count:
                    return "Count must be positive and count must refer to a location within the string/array/collection.";
                case ExceptionResource.ArgumentOutOfRange_Year:
                    return "Year must be between 1 and 9999.";
                case ExceptionResource.Arg_ArrayPlusOffTooSmall:
                    return "Destination array is not long enough to copy all the items in the collection. Check array index and length.";
                case ExceptionResource.NotSupported_ReadOnlyCollection:
                    return "Collection is read-only.";
                case ExceptionResource.Arg_RankMultiDimNotSupported:
                    return "Only single dimensional arrays are supported for the requested action.";
                case ExceptionResource.Arg_NonZeroLowerBound:
                    return "The lower bound of target array must be zero.";
                case ExceptionResource.ArgumentOutOfRange_GetCharCountOverflow:
                    return "Too many bytes. The resulting number of chars is larger than what can be returned as an int.";
                case ExceptionResource.ArgumentOutOfRange_ListInsert:
                    return "Index must be within the bounds of the List.";
                case ExceptionResource.ArgumentOutOfRange_NeedNonNegNum:
                    return "Non-negative number required.";
                case ExceptionResource.ArgumentOutOfRange_SmallCapacity:
                    return "capacity was less than the current size.";
                case ExceptionResource.Argument_InvalidOffLen:
                    return "Offset and length were out of bounds for the array or count is greater than the number of elements from index to the end of the source collection.";
                case ExceptionResource.Argument_CannotExtractScalar:
                    return "Cannot extract a Unicode scalar value from the specified index in the input.";
                case ExceptionResource.ArgumentOutOfRange_BiggerThanCollection:
                    return "Larger than collection size.";
                case ExceptionResource.Serialization_MissingKeys:
                    return "The Keys for this Hashtable are missing.";
                case ExceptionResource.Serialization_NullKey:
                    return "One of the serialized keys is null.";
                case ExceptionResource.NotSupported_KeyCollectionSet:
                    return "Mutating a key collection derived from a dictionary is not allowed.";
                case ExceptionResource.NotSupported_ValueCollectionSet:
                    return "Mutating a value collection derived from a dictionary is not allowed.";
                case ExceptionResource.InvalidOperation_NullArray:
                    return "The underlying array is null.";
                case ExceptionResource.TaskT_TransitionToFinal_AlreadyCompleted:
                    return "An attempt was made to transition a task to a final state when it had already completed.";
                case ExceptionResource.TaskCompletionSourceT_TrySetException_NullException:
                    return "The exceptions collection included at least one null element.";
                case ExceptionResource.TaskCompletionSourceT_TrySetException_NoExceptions:
                    return "The exceptions collection was empty.";
                case ExceptionResource.NotSupported_StringComparison:
                    return "The string comparison type passed in is currently not supported.";
                case ExceptionResource.ConcurrentCollection_SyncRoot_NotSupported:
                    return "The SyncRoot property may not be used for the synchronization of concurrent collections.";
                case ExceptionResource.Task_MultiTaskContinuation_NullTask:
                    return "The tasks argument included a null value.";
                case ExceptionResource.InvalidOperation_WrongAsyncResultOrEndCalledMultiple:
                    return "Either the IAsyncResult object did not come from the corresponding async method on this type, or the End method was called multiple times with the same IAsyncResult.";
                case ExceptionResource.Task_MultiTaskContinuation_EmptyTaskList:
                    return "The tasks argument contains no tasks.";
                case ExceptionResource.Task_Start_TaskCompleted:
                    return "Start may not be called on a task that has completed.";
                case ExceptionResource.Task_Start_Promise:
                    return "Start may not be called on a promise-style task.";
                case ExceptionResource.Task_Start_ContinuationTask:
                    return "Start may not be called on a continuation task.";
                case ExceptionResource.Task_Start_AlreadyStarted:
                    return "Start may not be called on a task that was already started.";
                case ExceptionResource.Task_RunSynchronously_Continuation:
                    return "RunSynchronously may not be called on a continuation task.";
                case ExceptionResource.Task_RunSynchronously_Promise:
                    return "RunSynchronously may not be called on a task not bound to a delegate, such as the task returned from an asynchronous method.";
                case ExceptionResource.Task_RunSynchronously_TaskCompleted:
                    return "RunSynchronously may not be called on a task that has already completed.";
                case ExceptionResource.Task_RunSynchronously_AlreadyStarted:
                    return "RunSynchronously may not be called on a task that was already started.";
                case ExceptionResource.AsyncMethodBuilder_InstanceNotInitialized:
                    return "The builder was not properly initialized.";
                case ExceptionResource.Task_ContinueWith_ESandLR:
                    return "The specified TaskContinuationOptions combined LongRunning and ExecuteSynchronously.  Synchronous continuations should not be long running.";
                case ExceptionResource.Task_ContinueWith_NotOnAnything:
                    return "The specified TaskContinuationOptions excluded all continuation kinds.";
                case ExceptionResource.Task_InvalidTimerTimeSpan:
                    return "The value needs to translate in milliseconds to -1 (signifying an infinite timeout), 0, or a positive integer less than or equal to the maximum allowed timer duration.";
                case ExceptionResource.Task_Delay_InvalidMillisecondsDelay:
                    return "The value needs to be either -1 (signifying an infinite timeout), 0 or a positive integer.";
                case ExceptionResource.Task_Dispose_NotCompleted:
                    return "A task may only be disposed if it is in a completion state (RanToCompletion, Faulted or Canceled).";
                case ExceptionResource.Task_ThrowIfDisposed:
                    return "The task has been disposed.";
                case ExceptionResource.Task_WaitMulti_NullTask:
                    return "The tasks array included at least one null element.";
                case ExceptionResource.ArgumentException_OtherNotArrayOfCorrectLength:
                    return "The object is not an array with the same number of elements as the array to compare it to.";
                case ExceptionResource.ArgumentNull_Array:
                    return "Array cannot be null.";
                case ExceptionResource.ArgumentNull_SafeHandle:
                    return "SafeHandle cannot be null.";
                case ExceptionResource.ArgumentOutOfRange_EndIndexStartIndex:
                    return "endIndex cannot be greater than startIndex.";
                case ExceptionResource.ArgumentOutOfRange_Enum:
                    return "Enum value was out of legal range.";
                case ExceptionResource.ArgumentOutOfRange_HugeArrayNotSupported:
                    return "Arrays larger than 2GB are not supported.";
                case ExceptionResource.Argument_AddingDuplicate:
                    return "An item with the same key has already been added.";
                case ExceptionResource.Argument_InvalidArgumentForComparison:
                    return "Type of argument is not compatible with the generic comparer.";
                case ExceptionResource.Arg_LowerBoundsMustMatch:
                    return "The arrays' lower bounds must be identical.";
                case ExceptionResource.Arg_MustBeType:
                    return "Type must be a type provided by the runtime.";
                case ExceptionResource.Arg_Need1DArray:
                    return "Array was not a one-dimensional array.";
                case ExceptionResource.Arg_Need2DArray:
                    return "Array was not a two-dimensional array.";
                case ExceptionResource.Arg_Need3DArray:
                    return "Array was not a three-dimensional array.";
                case ExceptionResource.Arg_NeedAtLeast1Rank:
                    return "Must provide at least one rank.";
                case ExceptionResource.Arg_RankIndices:
                    return "Indices length does not match the array rank.";
                case ExceptionResource.Arg_RanksAndBounds:
                    return "Number of lengths and lowerBounds must match.";
                case ExceptionResource.InvalidOperation_IComparerFailed:
                    return "Failed to compare two elements in the array.";
                case ExceptionResource.NotSupported_FixedSizeCollection:
                    return "Collection was of a fixed size.";
                case ExceptionResource.Rank_MultiDimNotSupported:
                    return "Only single dimension arrays are supported here.";
                case ExceptionResource.Arg_TypeNotSupported:
                    return "Specified type is not supported";
                case ExceptionResource.Argument_SpansMustHaveSameLength:
                    return "Length of items must be same as length of keys.";
                case ExceptionResource.Argument_InvalidFlag:
                    return "Value of flags is invalid.";
                case ExceptionResource.CancellationTokenSource_Disposed:
                    return "The CancellationTokenSource has been disposed.";
                case ExceptionResource.Argument_AlignmentMustBePow2:
                    return "The alignment must be a power of two.";
                case ExceptionResource.ArgumentOutOfRange_NotGreaterThanBufferLength:
                    return "Must not be greater than the length of the buffer.";
                default:
                    // TODO: Debug.Fail("The enum value is not defined, please check the ExceptionResource Enum.");
                    return "";
            }
        }
    }

    //
    // The convention for this enum is using the argument name as the enum name
    //
    internal enum ExceptionArgument
    {
        obj,
        dictionary,
        array,
        info,
        key,
        text,
        values,
        value,
        startIndex,
        task,
        bytes,
        byteIndex,
        byteCount,
        ch,
        chars,
        charIndex,
        charCount,
        s,
        input,
        ownedMemory,
        list,
        index,
        capacity,
        collection,
        item,
        converter,
        match,
        count,
        action,
        comparison,
        exceptions,
        exception,
        pointer,
        start,
        format,
        formats,
        culture,
        comparer,
        comparable,
        source,
        state,
        length,
        comparisonType,
        manager,
        sourceBytesToCopy,
        callBack,
        creationOptions,
        function,
        scheduler,
        continuationAction,
        continuationFunction,
        tasks,
        asyncResult,
        beginMethod,
        endMethod,
        endFunction,
        cancellationToken,
        continuationOptions,
        delay,
        millisecondsDelay,
        millisecondsTimeout,
        stateMachine,
        timeout,
        type,
        sourceIndex,
        sourceArray,
        destinationIndex,
        destinationArray,
        pHandle,
        handle,
        other,
        newSize,
        lowerBounds,
        lengths,
        len,
        keys,
        indices,
        index1,
        index2,
        index3,
        length1,
        length2,
        length3,
        endIndex,
        elementType,
        arrayIndex,
        year,
        codePoint,
        str,
        options,
        prefix,
        suffix,
        buffer,
        buffers,
        offset,
        stream,
        anyOf,
        overlapped,
        minimumBytes,
    }

    //
    // The convention for this enum is using the resource name as the enum name
    //
    internal enum ExceptionResource
    {
        ArgumentOutOfRange_Index,
        ArgumentOutOfRange_IndexMustBeLessOrEqual,
        ArgumentOutOfRange_IndexMustBeLess,
        ArgumentOutOfRange_IndexCount,
        ArgumentOutOfRange_IndexCountBuffer,
        ArgumentOutOfRange_Count,
        ArgumentOutOfRange_Year,
        Arg_ArrayPlusOffTooSmall,
        NotSupported_ReadOnlyCollection,
        Arg_RankMultiDimNotSupported,
        Arg_NonZeroLowerBound,
        ArgumentOutOfRange_GetCharCountOverflow,
        ArgumentOutOfRange_ListInsert,
        ArgumentOutOfRange_NeedNonNegNum,
        ArgumentOutOfRange_NotGreaterThanBufferLength,
        ArgumentOutOfRange_SmallCapacity,
        Argument_InvalidOffLen,
        Argument_CannotExtractScalar,
        ArgumentOutOfRange_BiggerThanCollection,
        Serialization_MissingKeys,
        Serialization_NullKey,
        NotSupported_KeyCollectionSet,
        NotSupported_ValueCollectionSet,
        InvalidOperation_NullArray,
        TaskT_TransitionToFinal_AlreadyCompleted,
        TaskCompletionSourceT_TrySetException_NullException,
        TaskCompletionSourceT_TrySetException_NoExceptions,
        NotSupported_StringComparison,
        ConcurrentCollection_SyncRoot_NotSupported,
        Task_MultiTaskContinuation_NullTask,
        InvalidOperation_WrongAsyncResultOrEndCalledMultiple,
        Task_MultiTaskContinuation_EmptyTaskList,
        Task_Start_TaskCompleted,
        Task_Start_Promise,
        Task_Start_ContinuationTask,
        Task_Start_AlreadyStarted,
        Task_RunSynchronously_Continuation,
        Task_RunSynchronously_Promise,
        Task_RunSynchronously_TaskCompleted,
        Task_RunSynchronously_AlreadyStarted,
        AsyncMethodBuilder_InstanceNotInitialized,
        Task_ContinueWith_ESandLR,
        Task_ContinueWith_NotOnAnything,
        Task_InvalidTimerTimeSpan,
        Task_Delay_InvalidMillisecondsDelay,
        Task_Dispose_NotCompleted,
        Task_ThrowIfDisposed,
        Task_WaitMulti_NullTask,
        ArgumentException_OtherNotArrayOfCorrectLength,
        ArgumentNull_Array,
        ArgumentNull_SafeHandle,
        ArgumentOutOfRange_EndIndexStartIndex,
        ArgumentOutOfRange_Enum,
        ArgumentOutOfRange_HugeArrayNotSupported,
        Argument_AddingDuplicate,
        Argument_InvalidArgumentForComparison,
        Arg_LowerBoundsMustMatch,
        Arg_MustBeType,
        Arg_Need1DArray,
        Arg_Need2DArray,
        Arg_Need3DArray,
        Arg_NeedAtLeast1Rank,
        Arg_RankIndices,
        Arg_RanksAndBounds,
        InvalidOperation_IComparerFailed,
        NotSupported_FixedSizeCollection,
        Rank_MultiDimNotSupported,
        Arg_TypeNotSupported,
        Argument_SpansMustHaveSameLength,
        Argument_InvalidFlag,
        CancellationTokenSource_Disposed,
        Argument_AlignmentMustBePow2,
    }
}
